package fr.livetek.fruityprime;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

public final class InstallResultReceiver extends BroadcastReceiver {
    static {
        loadGameLibrary();
        nativeBind();
    }

    // Qt packages the game as libFruityPrime_<abi>.so and has already loaded
    // it by the time the menus are READY; loading it again by that name is a
    // no-op that makes its natives visible here. The bare name was the
    // pre-Qt package's and does not exist in the APK any more: asking for it
    // threw out of the READY callback, so nativeCreate never ran and the
    // launcher stayed black on every device.
    static void loadGameLibrary() {
        UnsatisfiedLinkError last = null;
        for (String abi : Build.SUPPORTED_ABIS) {
            try {
                System.loadLibrary("FruityPrime_" + abi);
                return;
            } catch (UnsatisfiedLinkError missing) {
                last = missing;
            }
        }
        try {
            System.loadLibrary("FruityPrime");
        } catch (UnsatisfiedLinkError missing) {
            throw last != null ? last : missing;
        }
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
