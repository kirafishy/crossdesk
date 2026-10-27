import SwiftUI

struct PrivacyConsentView: View {
    @ObservedObject var session: RemoteSessionModel
    @State private var hasReadPolicy = false

    var body: some View {
        NavigationStack {
            ScrollView {
                PrivacyPolicyDocument()
                    .padding(.horizontal, 20)
                    .padding(.vertical, 20)
                    .readableContentWidth()
            }
            .background(Color(.systemBackground))
            .safeAreaInset(edge: .bottom, spacing: 0) {
                consentActions
            }
            .navigationTitle(NSLocalizedString("隐私政策", comment: ""))
            .navigationBarTitleDisplayMode(.inline)
        }
    }

    private var consentActions: some View {
        VStack(spacing: 0) {
            Divider()

            VStack(spacing: 12) {
                Toggle(isOn: $hasReadPolicy) {
                    Text(NSLocalizedString("我已阅读并同意《隐私政策》", comment: ""))
                        .font(.subheadline)
                }
                .toggleStyle(ConsentCheckboxStyle())

                Button {
                    guard hasReadPolicy else { return }
                    session.acceptNetworkConsent()
                } label: {
                    Text(NSLocalizedString("同意并继续", comment: ""))
                        .font(.body.weight(.semibold))
                        .frame(maxWidth: .infinity, minHeight: 24)
                }
                .buttonStyle(.borderedProminent)
                .controlSize(.large)
                .tint(.blue)
                .disabled(!hasReadPolicy || !PrivacyPolicyDocument.isAvailable)

                Button {
                    session.declineNetworkConsent()
                } label: {
                    Text(NSLocalizedString("暂不同意", comment: ""))
                        .font(.body)
                        .frame(maxWidth: .infinity, minHeight: 44)
                        .contentShape(Rectangle())
                }
                .foregroundStyle(.secondary)
                .buttonStyle(.plain)
            }
            .padding(.horizontal, 20)
            .padding(.vertical, 12)
            .readableContentWidth()
        }
        .background(Color(.systemBackground))
    }
}

private struct ConsentCheckboxStyle: ToggleStyle {
    func makeBody(configuration: Configuration) -> some View {
        Button {
            configuration.isOn.toggle()
        } label: {
            HStack(alignment: .center, spacing: 9) {
                Image(systemName: configuration.isOn ? "checkmark.square.fill" : "square")
                    .font(.title3)
                    .foregroundStyle(configuration.isOn ? Color.accentColor : .secondary)
                configuration.label
                    .foregroundStyle(.primary)
                    .multilineTextAlignment(.leading)
            }
            .frame(maxWidth: .infinity, minHeight: 44, alignment: .leading)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
        .accessibilityLabel(NSLocalizedString("我已阅读并同意隐私政策", comment: ""))
        .accessibilityValue(configuration.isOn ? NSLocalizedString("已勾选", comment: "") : NSLocalizedString("未勾选", comment: ""))
    }
}

struct PrivacySettingsSection: View {
    @ObservedObject var session: RemoteSessionModel
    @State private var confirmThumbnails = false
    @State private var confirmClear = false

    var body: some View {
        Section {
            NavigationLink {
                PrivacyAuthorizationView(session: session)
            } label: {
                HStack {
                    Text(NSLocalizedString("隐私与授权", comment: ""))
                    Spacer()
                    Text(session.hasNetworkConsent ? NSLocalizedString("已授权", comment: "") : NSLocalizedString("未授权", comment: ""))
                        .foregroundStyle(.secondary)
                }
            }
            Toggle(NSLocalizedString("保存远程画面预览", comment: ""), isOn: Binding(
                get: { session.savesConnectionThumbnails },
                set: { enabled in
                    if enabled { confirmThumbnails = true }
                    else { session.setSavesConnectionThumbnails(false) }
                }
            ))
            .disabled(!session.hasNetworkConsent || session.thumbnailCleanupInProgress)
            Button(session.thumbnailCleanupInProgress ? NSLocalizedString("正在清除…", comment: "") : NSLocalizedString("清除预览图", comment: ""), role: .destructive) {
                confirmClear = true
            }
            .disabled(session.thumbnailCleanupInProgress)
            if let error = session.thumbnailCleanupError {
                Text(error).font(.footnote).foregroundStyle(.secondary)
            }
        } header: {
            Text(NSLocalizedString("隐私", comment: ""))
        }
        .alert(NSLocalizedString("保存远程画面预览？", comment: ""), isPresented: $confirmThumbnails) {
            Button(NSLocalizedString("开启保存", comment: "")) { session.setSavesConnectionThumbnails(true) }
            Button(NSLocalizedString("取消", comment: ""), role: .cancel) { }
        } message: {
            Text(NSLocalizedString("预览可能包含远程电脑上的个人或工作信息。开启后，每次连接会自动将一张画面保存在本机并显示在最近连接中。你可以随时关闭并清除。", comment: ""))
        }
        .alert(NSLocalizedString("清除预览图？", comment: ""), isPresented: $confirmClear) {
            Button(NSLocalizedString("清除", comment: ""), role: .destructive) { session.clearConnectionThumbnails() }
            Button(NSLocalizedString("取消", comment: ""), role: .cancel) { }
        } message: {
            Text(session.savesConnectionThumbnails
                 ? NSLocalizedString("将删除本机保存的所有远程预览图，保留连接记录、密码和接收的文件。后续连接仍会保存新的预览图。", comment: "")
                 : NSLocalizedString("将删除本机保存的所有远程预览图，保留连接记录、密码和接收的文件。", comment: ""))
        }
    }
}

private struct PrivacyAuthorizationView: View {
    @ObservedObject var session: RemoteSessionModel
    @State private var confirmWithdrawal = false

    var body: some View {
        Form {
            Section {
                LabeledContent(NSLocalizedString("授权状态", comment: ""), value: session.hasNetworkConsent ? NSLocalizedString("已授权", comment: "") : NSLocalizedString("未授权", comment: ""))
                NavigationLink(NSLocalizedString("隐私政策", comment: "")) { PrivacyPolicyView() }
            } footer: {
                if !session.hasNetworkConsent {
                    Text(NSLocalizedString("授权后可使用远程连接。", comment: ""))
                }
            }

            Section {
                if session.hasNetworkConsent {
                    Button(NSLocalizedString("撤回隐私授权", comment: ""), role: .destructive) {
                        confirmWithdrawal = true
                    }
                } else {
                    Button(NSLocalizedString("阅读并授权", comment: "")) { session.showPrivacyNotice() }
                }
            }
        }
        .readableContentWidth()
        .background(Color(.systemGroupedBackground).ignoresSafeArea())
        .navigationTitle(NSLocalizedString("隐私与授权", comment: ""))
        .navigationBarTitleDisplayMode(.inline)
        .alert(NSLocalizedString("是否撤回隐私授权？", comment: ""), isPresented: $confirmWithdrawal) {
            Button(NSLocalizedString("撤回授权", comment: ""), role: .destructive) { session.revokeNetworkConsent() }
            Button(NSLocalizedString("取消", comment: ""), role: .cancel) { }
        } message: {
            Text(NSLocalizedString("将停止远程连接、关闭画面预览保存并清除已有预览。连接记录和接收文件会保留，已有服务端数据不会删除。您可随时重新授权。", comment: ""))
        }
    }
}

/// Bundled policy is readable before any network consent or external request.
private struct PrivacyPolicyDocument: View {
    private static let text: String? = {
        guard let url = Bundle.main.url(forResource: "PRIVACY", withExtension: "md"),
              let text = try? String(contentsOf: url, encoding: .utf8),
              !text.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else { return nil }
        // Keep the published policy intact; use its English version outside Chinese locales.
        if let start = text.range(of: "## 中文\n"),
           let end = text.range(of: "## English", range: start.upperBound..<text.endIndex) {
            let chinese = Bundle.main.preferredLocalizations.first?.hasPrefix("zh") == true
            return chinese ? String(text[start.upperBound..<end.lowerBound])
                : String(text[end.upperBound...])
        }
        return text
    }()
    static var isAvailable: Bool { text != nil }
    private static let paragraphs = (text ?? NSLocalizedString("隐私政策暂时无法读取，请拒绝联网并重新安装应用。", comment: ""))
        .components(separatedBy: "\n\n")
        .map { $0.trimmingCharacters(in: .whitespacesAndNewlines) }
        .filter { !$0.isEmpty && $0 != "---" }

    var body: some View {
        VStack(alignment: .leading, spacing: 14) {
            ForEach(Self.paragraphs.indices, id: \.self) { index in
                let paragraph = Self.paragraphs[index]
                if paragraph.hasPrefix("#") {
                    Text(paragraph.trimmingCharacters(in: CharacterSet(charactersIn: "# ")))
                        .font(.headline)
                        .padding(.top, 4)
                } else if paragraph.hasPrefix("更新日期：") || paragraph.hasPrefix("Last updated:") {
                    Text(paragraph)
                        .font(.caption)
                        .foregroundStyle(.secondary)
                } else {
                    Text((try? AttributedString(markdown: paragraph,
                        options: .init(interpretedSyntax: .inlineOnlyPreservingWhitespace)))
                         ?? AttributedString(paragraph))
                        .font(.body)
                        .lineSpacing(4)
                }
            }
        }
        .frame(maxWidth: .infinity, alignment: .leading)
        .textSelection(.enabled)
    }
}

struct PrivacyPolicyView: View {
    var body: some View {
        ScrollView {
            PrivacyPolicyDocument().padding().readableContentWidth()
        }
        .navigationTitle(NSLocalizedString("隐私政策", comment: ""))
        .navigationBarTitleDisplayMode(.inline)
    }
}
