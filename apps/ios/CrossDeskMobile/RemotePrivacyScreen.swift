import Foundation

struct RemotePrivacyStatus: Decodable {
    let state: Int
    let supported: Bool
    let inputBlockSupported: Bool
    let overlayActive: Bool
    let inputBlocked: Bool
    let remotePaused: Bool
    let revision: UInt32
    let reason: String
    let localUnlockRevision: UInt32?
}

struct RemotePrivacyScreen {
    private(set) var desktopAvailable = true
    private(set) var snapshot: RemotePrivacyStatus?
    var unlockNotice = false
    private(set) var pending = false
    private(set) var requestID: UInt32 = 0
    private var requestRevision: UInt32 = 0
    private var receivedAt: TimeInterval = 0
    private var requestedAt: TimeInterval = 0
    private var failure = ""

    mutating func setInteractiveStage(_ stage: String) {
        desktopAvailable = !["lock-screen", "credential-ui", "secure-desktop"].contains(stage)
    }

    var active: Bool {
        guard let s = snapshot else { return false }
        return s.overlayActive || s.remotePaused || [2, 3, 4].contains(s.state)
    }
    func fresh(at now: TimeInterval) -> Bool { snapshot != nil && now - receivedAt <= 5 }
    func enabled(at now: TimeInterval) -> Bool {
        guard let s = snapshot else { return false }
        return fresh(at: now) && s.state == 3 && s.overlayActive && !s.remotePaused
    }
    func buttonTitle(at now: TimeInterval) -> String {
        if enabled(at: now) { return "隐私屏已开启" }
        if fresh(at: now), let s = snapshot, s.supported && s.state == 1 && !active {
            return "隐私屏已关闭"
        }
        return "隐私屏"
    }
    func feedback(at now: TimeInterval) -> String {
        if !desktopAvailable { return "" }
        if failure.isEmpty && (pending || fresh(at: now) && [2, 4].contains(snapshot?.state ?? -1)) {
            return ""
        }
        if failure.isEmpty && !pending && fresh(at: now), let s = snapshot, s.supported,
           enabled(at: now) || (s.state == 1 && !active) { return "" }
        let reason = detail(at: now)
        return status(at: now) + (reason.isEmpty ? "" : "：" + reason)
    }
    func canToggle(at now: TimeInterval) -> Bool {
        guard let s = snapshot else { return false }
        return desktopAvailable && !pending && fresh(at: now) && s.state != 4 && (active || s.supported && s.state == 1)
    }
    mutating func receive(_ data: Data, at now: TimeInterval) {
        struct Envelope: Decodable { let type: Int; let privacyStatus: RemotePrivacyStatus }
        let decoder = JSONDecoder(); decoder.keyDecodingStrategy = .convertFromSnakeCase
        guard let message = try? decoder.decode(Envelope.self, from: data), message.type == 10 else { return }
        let s = message.privacyStatus
        guard (0...5).contains(s.state), s.reason.utf8.count < 256,
              !s.reason.contains("\0"),
              s.state != 3 || (s.supported && s.overlayActive && !s.remotePaused) else { return }
        if let previous = snapshot, Int32(bitPattern: s.revision &- previous.revision) < 0 { return }
        if let previous = snapshot,
           Int32(bitPattern: (s.localUnlockRevision ?? 0) &- (previous.localUnlockRevision ?? 0)) > 0 {
            unlockNotice = true
        }
        if s.state == 3 { unlockNotice = false }
        snapshot = s; receivedAt = now; failure = ""
        if pending && Int32(bitPattern: s.revision &- requestRevision) > 0 && ![2, 4].contains(s.state) {
            pending = false
        }
    }
    func canLock(at now: TimeInterval) -> Bool {
        desktopAvailable && !pending && fresh(at: now) && snapshot?.supported == true
    }
    mutating func begin(at now: TimeInterval, lock: Bool = false) -> UInt32? {
        tick(at: now)
        guard (lock ? canLock(at: now) : canToggle(at: now)), let s = snapshot else { return nil }
        pending = true; requestedAt = now; requestRevision = s.revision; failure = ""
        requestID &+= 1
        return requestID
    }
    mutating func failed(request: UInt32) {
        if pending && requestID == request { pending = false; failure = "指令发送失败，请重试" }
    }
    mutating func tick(at now: TimeInterval) {
        if pending && now - requestedAt >= 15 {
            pending = false; failure = "请求超时，尚未确认电脑的隐私屏状态"
        }
    }
    func status(at now: TimeInterval) -> String {
        if !failure.isEmpty { return failure }
        guard let s = snapshot else { return "等待电脑返回隐私屏状态；旧版电脑端可能不支持" }
        if !fresh(at: now) { return "隐私屏状态已过期，等待电脑确认" }
        if s.state == 2 { return "正在开启隐私屏…" }
        if s.state == 4 { return "正在关闭隐私屏…" }
        if pending { return "指令已发送，等待电脑确认…" }
        if s.state == 5 { return "隐私屏异常" }
        if !s.supported || s.state == 0 { return "隐私屏暂不可用" }
        if s.state == 3 && s.overlayActive && !s.remotePaused {
            return "隐私屏已开启"
        }
        return s.state == 1 && !active ? "隐私屏已关闭" : "隐私屏状态待确认"
    }
    func detail(at now: TimeInterval) -> String {
        guard let s = snapshot, fresh(at: now), s.state == 0 || s.state == 5 || !s.supported else { return "" }
        return s.reason
    }
}
