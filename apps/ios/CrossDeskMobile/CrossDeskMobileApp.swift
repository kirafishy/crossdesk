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
    private static var sessionActive = false
    private static var waitingForLandscape = false
    private static var deviceObserver: NSObjectProtocol?

    nonisolated static func beginSession(autoLandscape: Bool) {
        DispatchQueue.main.async { beginSessionOnMain(autoLandscape: autoLandscape) }
    }

    private static func beginSessionOnMain(autoLandscape: Bool) {
        // Repeated transport-ready callbacks must not lock an existing session again.
        guard !sessionActive else { return }
        sessionActive = true
        waitingForLandscape = autoLandscape && UIDevice.current.userInterfaceIdiom == .phone
        if waitingForLandscape {
            CrossDeskAppDelegate.supportedOrientations = .landscape
            resumeObserving()
        } else {
            apply(to: .allButUpsideDown)
        }
    }

    nonisolated static func endSession() {
        DispatchQueue.main.async {
            sessionActive = false
            waitingForLandscape = false
            stopObserving()
            apply(to: .portrait)
        }
    }

    nonisolated static func resume() {
        DispatchQueue.main.async { resumeObserving() }
    }

    private static func resumeObserving() {
        guard sessionActive, waitingForLandscape, deviceObserver == nil,
              UIApplication.shared.applicationState == .active else { return }
        apply(to: .landscape)
        deviceObserver = NotificationCenter.default.addObserver(
            forName: UIDevice.orientationDidChangeNotification, object: nil, queue: .main
        ) { _ in
            MainActor.assumeIsolated { deviceDidRotate() }
        }
        UIDevice.current.beginGeneratingDeviceOrientationNotifications()
        deviceDidRotate()
    }

    nonisolated static func suspend() {
        DispatchQueue.main.async { stopObserving() }
    }

    private static func stopObserving() {
        guard let observer = deviceObserver else { return }
        NotificationCenter.default.removeObserver(observer)
        deviceObserver = nil
        UIDevice.current.endGeneratingDeviceOrientationNotifications()
    }

    private static func deviceDidRotate() {
        // UIDevice reports physical posture; window geometry also changes when
        // we request landscape and must never release the initial lock itself.
        guard sessionActive, waitingForLandscape, deviceObserver != nil,
              UIApplication.shared.applicationState == .active,
              UIDevice.current.orientation.isLandscape else { return }
        waitingForLandscape = false
        stopObserving()
        apply(to: .allButUpsideDown)
    }

    private static func apply(to orientations: UIInterfaceOrientationMask) {
        // iPad keeps every orientation available, including in resizable windows.
        guard UIDevice.current.userInterfaceIdiom != .pad else { return }
        CrossDeskAppDelegate.supportedOrientations = orientations
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
