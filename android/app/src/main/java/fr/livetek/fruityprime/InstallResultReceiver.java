package fr.livetek.fruityprime;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

public final class InstallResultReceiver extends BroadcastReceiver {
    static {
        System.loadLibrary("FruityPrime");
        nativeBind();
    }

    public static void ensureBound() {
        // Referencing this method forces the class initializer above to bind
        // the native receiver class before ApkInstaller creates its Intent.
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        nativeOnReceive(context, intent);
    }

    private static native void nativeBind();
    private static native void nativeOnReceive(Context context, Intent intent);
}
