package cn.crossdesk.mobile;

import android.app.Activity;
import android.content.pm.ActivityInfo;
import android.view.OrientationEventListener;

/** Holds the initial landscape until the phone itself has been turned sideways. */
final class RemoteOrientation {
    private final Activity activity;
    private final OrientationEventListener sensor;
    private boolean waitingForLandscape, resumed;

    RemoteOrientation(Activity activity) {
        this.activity = activity;
        sensor = new OrientationEventListener(activity) {
            @Override public void onOrientationChanged(int degrees) {
                if (!resumed || !waitingForLandscape || activity.isInPictureInPictureMode()) return;
                // Use physical orientation, never the window rotated by our own request.
                // Ignore flat/unknown poses and keep a margin away from the diagonal.
                if ((degrees >= 60 && degrees <= 120) || (degrees >= 240 && degrees <= 300)) {
                    followDevice();
                }
            }
        };
    }

    void begin(boolean autoLandscape) {
        sensor.disable();
        waitingForLandscape = autoLandscape && sensor.canDetectOrientation();
        apply();
        if (resumed && waitingForLandscape) sensor.enable();
    }

    boolean waitingForLandscape() { return waitingForLandscape; }

    void resume() {
        resumed = true;
        if (waitingForLandscape && !activity.isInPictureInPictureMode()) sensor.enable();
    }

    void pause() { resumed = false; sensor.disable(); }

    void end() { waitingForLandscape = false; sensor.disable(); }

    void followDevice() {
        waitingForLandscape = false;
        sensor.disable();
        apply();
    }

    private void apply() {
        int orientation = waitingForLandscape
            ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            : ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR;
        if (activity.getRequestedOrientation() != orientation) activity.setRequestedOrientation(orientation);
    }
}
