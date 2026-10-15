#include <mobile_file_transfer_session.h>
#include <cassert>
#include <deque>
#include <iostream>
#include <unistd.h>
using namespace crossdesk;
using mobile_controller::FileTransferSession;
int main() {
  char pattern[] = "/tmp/crossdesk-mobile-files-XXXXXX";
  const std::filesystem::path root = mkdtemp(pattern);
  std::deque<std::string> chunks, acks;
  std::string received; bool complete = false, rejected = false;
  FileTransferSession sender([&](const char*, const char* p, size_t n) { chunks.emplace_back(p,n); return 0; },
    [&](const std::string&, double p, bool, const std::string&) { complete = p == 1; rejected |= p < 0; });
  FileTransferSession receiver([&](const char*, const char* p, size_t n) { acks.emplace_back(p,n); return 0; },
    [&](const std::string&, double p, bool, const std::string& path) { if (p == 1) received = path; });
  receiver.SetDirectory(root / "received");
  const std::string data(3*kFileChunkSize+37,'x');
  const auto staged = root / "outgoing";
  { std::ofstream out(staged,std::ios::binary); out << data; }
  assert(sender.Start(staged,"../../test.bin"));
  for(int i=0;i<4;i++) sender.Pump();
  assert(chunks.size()==4);
  // Real KCP callbacks may be reordered: do not expose holes as a finished file.
  for(int i : {3,1,0,2}) { const auto& c=chunks[i]; receiver.Receive(c.data(),c.size()); if(i!=2) assert(received.empty()); }
  assert(!received.empty() && std::filesystem::path(received).filename()=="test.bin");
  for(auto& a:acks) sender.Ack(a.data(),a.size());
  assert(complete && !sender.Sending() && !std::filesystem::exists(staged));
  std::ifstream input(received,std::ios::binary); const std::string result((std::istreambuf_iterator<char>(input)),{});
  assert(result == data);
  // Duplicate ranges must be rejected and their partial file removed.
  const std::string name="duplicate"; auto chunk=EncodeFileChunk(50,0,4,"ab",2,&name,true,false);
  receiver.Receive(chunk.data(),chunk.size()); receiver.Receive(chunk.data(),chunk.size());
  FileTransferAck ack{}; assert(DecodeFileTransferAck(acks.back().data(),acks.back().size(),&ack)); assert(ack.flags==2);
  // Sender timeout removes only the disposable staging copy.
  { std::ofstream out(staged); out << "timeout"; }
  const auto now=FileTransferSession::Clock::now(); assert(sender.Start(staged,"timeout",now));
  sender.Pump(now+std::chrono::seconds(31)); assert(rejected && !sender.Sending() && !std::filesystem::exists(staged));
  // Empty files still require terminal acknowledgement.
  chunks.clear(); acks.clear(); complete=false; received.clear();
  { std::ofstream out(staged); } assert(sender.Start(staged,"empty")); sender.Pump();
  receiver.Receive(chunks[0].data(),chunks[0].size()); assert(!received.empty());
  sender.Ack(acks[0].data(),acks[0].size()); assert(complete && std::filesystem::file_size(received)==0);
  const auto previous=received;
  {
    FileTransferSession fresh([](const char*,const char*,size_t){return 0;},
      [&](const std::string&,double p,bool,const std::string&path){if(p==1)received=path;});
    fresh.SetDirectory(root/"received");
    fresh.Receive(chunks[0].data(),chunks[0].size());
    assert(received!=previous && std::filesystem::exists(previous) && std::filesystem::exists(received));
  }
  std::filesystem::remove_all(root);
  std::cout << "PASS: shared Android/Harmony file transfer, reordered chunks, safe names, duplicate rejection, timeout cleanup and empty files\n";
}
