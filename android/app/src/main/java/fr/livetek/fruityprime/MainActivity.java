package fr.livetek.fruityprime;

import android.content.Context;
import android.content.res.AssetManager;
import android.content.res.Configuration;
import android.hardware.display.DisplayManager;
import android.hardware.input.InputManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.TextView;
import android.widget.Toast;

import android.app.Activity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public final class MainActivity extends Activity
        implements DisplayManager.DisplayListener, InputManager.InputDeviceListener {

    private long nativeHandle;
    private FrameLayout root;
    private LauncherView launcher;
    private DisplayManager displayManager;
    private InputManager inputManager;
    private boolean destroyed;
    private boolean resumed;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        root = new FrameLayout(this);
        launcher = new LauncherView(this);
        root.addView(
                launcher,
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        launcher.setStatusChangeListener(new org.qtproject.qt.android.QtQmlStatusChangeListener() {
            @Override public void onStatusChanged(org.qtproject.qt.android.QtQmlStatus status) {
            if (status == org.qtproject.qt.android.QtQmlStatus.READY) {
                runOnUiThread(() -> {
                    if (!destroyed && nativeHandle == 0) {
                        InstallResultReceiver.ensureBound();
                        nativeHandle = nativeCreate(savedInstanceState, root, launcher);
                        if (resumed) nativeOnResume(nativeHandle);
                        nativeOnWindowFocusChanged(nativeHandle, hasWindowFocus());
                    }
                });
            } else if (status == org.qtproject.qt.android.QtQmlStatus.ERROR) {
                runOnUiThread(() -> Toast.makeText(MainActivity.this, "Could not load the Qt menus", Toast.LENGTH_LONG).show());
            }
            }
        });
        setContentView(root);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        if (nativeHandle != 0) {
            nativeOnConfigurationChanged(nativeHandle, newConfig);
        }
    }

    @Override
    protected void onPause() {
        resumed = false;
        if (nativeHandle != 0) {
            nativeOnPause(nativeHandle);
        }
        super.onPause();
    }

    @Override
    protected void onResume() {
        super.onResume();
        resumed = true;
        if (nativeHandle != 0) {
            nativeOnResume(nativeHandle);
        }
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (nativeHandle != 0) {
            nativeOnWindowFocusChanged(nativeHandle, hasFocus);
        }
    }

    @Override
    protected void onDestroy() {
        destroyed = true;
        final long handle = nativeHandle;
        nativeHandle = 0;
        if (handle != 0) {
            nativeDestroy(handle);
        }
        super.onDestroy();
    }

    @Override
    @SuppressWarnings("deprecation")
    public void onBackPressed() {
        if (nativeHandle != 0) {
            nativeOnBackPressed(nativeHandle);
        } else {
            super.onBackPressed();
        }
    }

    @SuppressWarnings("deprecation")
    public void fruitySuperOnBackPressed() {
        super.onBackPressed();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (nativeHandle != 0 && nativeDispatchKeyEvent(nativeHandle, event)) {
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event) {
        if (nativeHandle != 0 && nativeDispatchTouchEvent(nativeHandle, event)) {
            return true;
        }
        return super.dispatchTouchEvent(event);
    }

    @Override
    public boolean dispatchGenericMotionEvent(MotionEvent event) {
        if (nativeHandle != 0 && nativeDispatchGenericMotionEvent(nativeHandle, event)) {
            return true;
        }
        return super.dispatchGenericMotionEvent(event);
    }

    @Override
    public void onDisplayAdded(int displayId) {
        if (nativeHandle != 0) {
            nativeOnDisplayAdded(nativeHandle, displayId);
        }
    }

    @Override
    public void onDisplayRemoved(int displayId) {
        if (nativeHandle != 0) {
            nativeOnDisplayRemoved(nativeHandle, displayId);
        }
    }

    @Override
    public void onDisplayChanged(int displayId) {
        if (nativeHandle != 0) {
            nativeOnDisplayChanged(nativeHandle, displayId);
        }
    }

    @Override
    public void onInputDeviceAdded(int deviceId) {
        if (nativeHandle != 0) {
            nativeOnInputDeviceAdded(nativeHandle, deviceId);
        }
    }

    @Override
    public void onInputDeviceChanged(int deviceId) {
        if (nativeHandle != 0) {
            nativeOnInputDeviceChanged(nativeHandle, deviceId);
        }
    }

    @Override
    public void onInputDeviceRemoved(int deviceId) {
        if (nativeHandle != 0) {
            nativeOnInputDeviceRemoved(nativeHandle, deviceId);
        }
    }

    public String fruityExternalFilesPath() {
        final File directory = getExternalFilesDir(null);
        return directory == null ? null : directory.getAbsolutePath();
    }

    public String fruityInternalFilesPath() {
        final File directory = getFilesDir();
        return directory == null ? null : directory.getAbsolutePath();
    }

    public GameSurfaceView fruityCreateGameSurfaceView() {
        return new GameSurfaceView(this);
    }

    public TouchOverlayView fruityCreateTouchOverlayView() {
        return new TouchOverlayView(this);
    }

    public TextView fruityCreateNoticeTextView(
            String text,
            int textArgb,
            int backgroundArgb) {
        final TextView view = new TextView(this);
        view.setText(text);
        view.setTextColor(textArgb);
        view.setBackgroundColor(backgroundArgb);
        view.setGravity(Gravity.CENTER);
        view.setTextAlignment(View.TEXT_ALIGNMENT_CENTER);
        return view;
    }

    public void fruityShowToast(String text) {
        Toast.makeText(this, text, Toast.LENGTH_LONG).show();
    }

    public void fruityPostNativeTask(long token) {
        runOnUiThread(() -> nativeRunTask(token));
    }

    public void fruityPostNativeTaskDelayed(View view, long token, long milliseconds) {
        view.postDelayed(() -> nativeRunTask(token), Math.max(0L, milliseconds));
    }

    public void fruitySetDisplayListenerEnabled(boolean enabled) {
        if (displayManager == null) {
            displayManager = (DisplayManager) getSystemService(Context.DISPLAY_SERVICE);
        }
        if (displayManager == null) {
            return;
        }
        if (enabled) {
            displayManager.registerDisplayListener(this, new Handler(Looper.getMainLooper()));
        } else {
            displayManager.unregisterDisplayListener(this);
        }
    }

    public void fruitySetInputListenerEnabled(boolean enabled) {
        if (inputManager == null) {
            inputManager = (InputManager) getSystemService(Context.INPUT_SERVICE);
        }
        if (inputManager == null) {
            return;
        }
        if (enabled) {
            inputManager.registerInputDeviceListener(this, new Handler(Looper.getMainLooper()));
        } else {
            inputManager.unregisterInputDeviceListener(this);
        }
    }

    public void fruityPrepareAssets(String rootPath) throws IOException {
        final File rootDirectory = new File(rootPath);
        copyAssetTree(getAssets(), "Assets", new File(rootDirectory, "Assets"));
    }

    private static void copyAssetTree(
            AssetManager assets,
            String assetPath,
            File destination) throws IOException {
        final String[] children = assets.list(assetPath);
        if (children != null && children.length > 0) {
            if (!destination.isDirectory() && !destination.mkdirs() && !destination.isDirectory()) {
                throw new IOException("Could not create " + destination);
            }
            for (String child : children) {
                copyAssetTree(
                        assets,
                        assetPath + "/" + child,
                        new File(destination, child));
            }
            return;
        }

        final File parent = destination.getParentFile();
        if (parent != null && !parent.isDirectory() && !parent.mkdirs() && !parent.isDirectory()) {
            throw new IOException("Could not create " + parent);
        }

        try (InputStream input = assets.open(assetPath);
             FileOutputStream output = new FileOutputStream(destination, false)) {
            final byte[] buffer = new byte[64 * 1024];
            int read;
            while ((read = input.read(buffer)) >= 0) {
                if (read != 0) {
                    output.write(buffer, 0, read);
                }
            }
        }
    }

    private native long nativeCreate(
            Bundle savedInstanceState,
            FrameLayout root,
            LauncherView launcher);
    private native void nativeDestroy(long handle);
    private native void nativeOnConfigurationChanged(long handle, Configuration configuration);
    private native void nativeOnPause(long handle);
    private native void nativeOnResume(long handle);
    private native void nativeOnWindowFocusChanged(long handle, boolean hasFocus);
    private native void nativeOnBackPressed(long handle);
    private native boolean nativeDispatchKeyEvent(long handle, KeyEvent event);
    private native boolean nativeDispatchTouchEvent(long handle, MotionEvent event);
    private native boolean nativeDispatchGenericMotionEvent(long handle, MotionEvent event);
    private native void nativeOnDisplayAdded(long handle, int displayId);
    private native void nativeOnDisplayRemoved(long handle, int displayId);
    private native void nativeOnDisplayChanged(long handle, int displayId);
    private native void nativeOnInputDeviceAdded(long handle, int deviceId);
    private native void nativeOnInputDeviceChanged(long handle, int deviceId);
    private native void nativeOnInputDeviceRemoved(long handle, int deviceId);

    private static native void nativeRunTask(long token);
}
