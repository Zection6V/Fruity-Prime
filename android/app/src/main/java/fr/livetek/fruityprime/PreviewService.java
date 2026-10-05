package fr.livetek.fruityprime;

import android.app.Service;
import android.content.Intent;
import android.os.IBinder;

// Renders a share of the room previews in a process of its own: the renderer
// keeps global state, so the launcher gets parallel previews by starting
// several of these (PreviewWorker0..9, each declared with its own
// android:process) rather than threads. The native peer does the work and
// stops the service when its share is drawn.
public abstract class PreviewService extends Service {
    static {
        InstallResultReceiver.loadGameLibrary();
    }

    private long nativeHandle;

    @Override
    public void onCreate() {
        super.onCreate();
        nativeHandle = nativeCreate(this);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        return nativeOnStartCommand(nativeHandle, intent, flags, startId);
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        // The share is drawn (the native peer stops the service only then).
        // A cached worker would keep a whole game process in memory, so end
        // it; the peer goes with it.
        android.os.Process.killProcess(android.os.Process.myPid());
    }

    private static native long nativeCreate(Service service);
    private static native int nativeOnStartCommand(long handle, Intent intent, int flags, int startId);
}
