import SwiftUI
import UIKit

final class CrossDeskAppDelegate: NSObject, UIApplicationDelegate {
    static var supportedOrientations: UIInterfaceOrientationMask =
        UIDevice.current.userInterfaceIdiom == .pad ? .all : .portrait

    func application(_ application: UIApplication,
                     supportedInterfaceOrientationsFor window: UIWindow?)
        -> UIInterfaceOrientationMask {
        Self.supportedOrientations
    }
}

@MainActor
enum AppOrientation {
    nonisolated static let autoLandscapeKey = "crossdesk.mobile.auto-landscape"
    private static var sessionOrientations: UIInterfaceOrientationMask?

    nonisolated static func beginSession(autoLandscape: Bool) {
        DispatchQueue.main.async { beginSessionOnMain(autoLandscape: autoLandscape) }
    }

    private static func beginSessionOnMain(autoLandscape: Bool) {
        // Preserve the selected policy through repeated transport-ready callbacks.
        guard sessionOrientations == nil else { return }
        let orientations: UIInterfaceOrientationMask = autoLandscape ? .landscape : .allButUpsideDown
        sessionOrientations = orientations
        apply(to: orientations)
    }

    nonisolated static func endSession() {
        DispatchQueue.main.async {
            sessionOrientations = nil
            apply(to: .portrait)
        }
    }

    nonisolated static func resume() {
        DispatchQueue.main.async {
            apply(to: sessionOrientations ?? .portrait)
        }
    }

    private static func apply(to orientations: UIInterfaceOrientationMask) {
        // iPad keeps every orientation available, including in resizable windows.
        guard UIDevice.current.userInterfaceIdiom != .pad else { return }
        CrossDeskAppDelegate.supportedOrientations = orientations
        guard UIApplication.shared.applicationState == .active else { return }
        for case let scene as UIWindowScene in UIApplication.shared.connectedScenes {
            scene.windows.forEach {
                $0.rootViewController?.setNeedsUpdateOfSupportedInterfaceOrientations()
            }
            scene.requestGeometryUpdate(.iOS(interfaceOrientations: orientations)) { error in
                NSLog("CrossDesk orientation update failed: %@", error.localizedDescription)
            }
        }
    }
}

@main
struct CrossDeskMobileApp: App {
    @UIApplicationDelegateAdaptor(CrossDeskAppDelegate.self) private var appDelegate
    @StateObject private var session = RemoteSessionModel()

    var body: some Scene {
        WindowGroup {
            ContentView(session: session)
        }
    }
}
