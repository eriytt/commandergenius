/* Copyright 2016 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.towersmatrix.vrx;

import com.google.vr.ndk.base.AndroidCompat;
import com.google.vr.ndk.base.GvrLayout;
import com.google.vr.ndk.base.GvrUiLayout;

import android.app.Activity;
import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Bundle;
import android.os.Vibrator;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.WindowManager;
import android.widget.Toast;

import java.net.*;
import java.util.*;  

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

import com.towersmatrix.vrx.xserver.VRXServer;

/**
 * A Gvr API sample application.
 */
public class VRXActivity extends Activity {
  private VRXServer xsrv = null;
  private GvrLayout gvrLayout;
  private long nativeVRXRenderer;

  static {
    System.loadLibrary("gvr");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    Unpacker up = new Unpacker(getFilesDir(), getAssets());
    if (!up.verifyInstallation())
	throw new RuntimeException("Runtime installation incomplete");

    xsrv = new VRXServer(getFilesDir().getPath());
    try {
      xsrv.launch();
    } catch (Throwable t) {
      throw new RuntimeException("xserver failed to launch", t);
    }

    // Ensure fullscreen immersion.
    setImmersiveSticky();
    getWindow()
        .getDecorView()
        .setOnSystemUiVisibilityChangeListener(
            new View.OnSystemUiVisibilityChangeListener() {
              @Override
              public void onSystemUiVisibilityChange(int visibility) {
                if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                  setImmersiveSticky();
                }
              }
            });

    // Initialize GvrLayout and the native renderer.
    gvrLayout = new GvrLayout(this);

    Log.i("VRX", "Creating native renderer");
    nativeVRXRenderer =
        nativeCreateRenderer(
            getClass().getClassLoader(),
            this.getApplicationContext(),
            gvrLayout.getGvrApi().getNativeGvrContext());

    // Add the GLSurfaceView to the GvrLayout.
    GLSurfaceView glSurfaceView = new GLSurfaceView(this);
    glSurfaceView.setEGLContextClientVersion(2);
    glSurfaceView.setEGLConfigChooser(8, 8, 8, 0, 0, 0);
    glSurfaceView.setPreserveEGLContextOnPause(true);
    glSurfaceView.setRenderer(
        new GLSurfaceView.Renderer() {
          @Override
          public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            nativeInitializeGl(nativeVRXRenderer);
          }

          @Override
          public void onSurfaceChanged(GL10 gl, int width, int height) {}

          @Override
          public void onDrawFrame(GL10 gl) {
            nativeDrawFrame(nativeVRXRenderer);
          }
        });
    // glSurfaceView.setOnTouchListener(
    //     new View.OnTouchListener() {
    //       @Override
    //       public boolean onTouch(View v, MotionEvent event) {
    //         if (event.getAction() == MotionEvent.ACTION_DOWN) {
    //           // Give user feedback and signal a trigger event.
    //           ((Vibrator) getSystemService(Context.VIBRATOR_SERVICE)).vibrate(50);
    //           nativeOnTriggerEvent(nativeVRXRenderer);
    //           return true;
    //         }
    //         return false;
    //       }
    //     });
    gvrLayout.setPresentationView(glSurfaceView);

    // Add the GvrLayout to the View hierarchy.
    setContentView(gvrLayout);

    // Enable scan line racing.
    if (gvrLayout.setAsyncReprojectionEnabled(true)) {
      // Scanline racing decouples the app framerate from the display framerate,
      // allowing immersive interaction even at the throttled clockrates set by
      // sustained performance mode.
      AndroidCompat.setSustainedPerformanceMode(this, true);
    }

    // Enable VR Mode.
    AndroidCompat.setVrModeEnabled(this, true);

    // Prevent screen from dimming/locking.
    getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  }

  @Override
  protected void onPause() {
    super.onPause();
    nativeOnPause(nativeVRXRenderer);
    gvrLayout.onPause();
  }

  @Override
  protected void onResume() {
    super.onResume();
    nativeOnResume(nativeVRXRenderer);
    gvrLayout.onResume();
    String ipInfo = new String("IP address: ");
    ipInfo += getIPAddress(true);
    Toast.makeText(this, ipInfo, Toast.LENGTH_LONG).show();
  }

  @Override
  protected void onDestroy() {
    super.onDestroy();
    // Destruction order is important; shutting down the GvrLayout will detach
    // the GLSurfaceView and stop the GL thread, allowing safe shutdown of
    // native resources from the UI thread.
    if (xsrv != null)
      xsrv.terminate();

    gvrLayout.shutdown();
    nativeDestroyRenderer(nativeVRXRenderer);
  }

  @Override
  public void onWindowFocusChanged(boolean hasFocus) {
    super.onWindowFocusChanged(hasFocus);
    if (hasFocus) {
      setImmersiveSticky();
    }
  }

  @Override
  public boolean dispatchKeyEvent(KeyEvent event) {
    Log.i("VRX", "Got key event: " + event);

    int action = event.getAction();
    if (action == KeyEvent.ACTION_MULTIPLE)
      return true;

    if (xsrv == null)
	return true;

    if (event.isAltPressed()
	&& event.getKeyCode() >= KeyEvent.KEYCODE_DPAD_UP
	&& event.getKeyCode() <= KeyEvent.KEYCODE_DPAD_RIGHT) {
	int amount = 5;
	if (event.isShiftPressed())
	    amount = 30;

	switch (event.getKeyCode()) {
	case KeyEvent.KEYCODE_DPAD_UP:
	    xsrv.nativeMouseMotionEvent(0, -amount);
	    break;
	case KeyEvent.KEYCODE_DPAD_DOWN:
	    xsrv.nativeMouseMotionEvent(0, amount);
	    break;
	case KeyEvent.KEYCODE_DPAD_RIGHT:
	    xsrv.nativeMouseMotionEvent(amount, 0);
	    break;
	case KeyEvent.KEYCODE_DPAD_LEFT:
	    xsrv.nativeMouseMotionEvent(-amount, 0);
	    break;
	}
	return true;
    }

    // if (event.getKeyCode() == KeyEvent.KEYCODE_P)
    //   xsrv.nativeMouseMotionEvent(100, 100);

    int keyTrapped = nativeWMEvent(nativeVRXRenderer, event.getScanCode(), action == KeyEvent.ACTION_DOWN);
    if (keyTrapped==1)
    {
      Log.i("VRX", "Key Trapped");
      return true;
    }
    
    xsrv.nativeKeyEvent(event.getScanCode(), action == KeyEvent.ACTION_DOWN);
    return true;
  }

  @Override
  public boolean dispatchGenericMotionEvent (MotionEvent event) {
    Log.i("VRX", "Got motion event: " + event);
    return true;
  }

  private void setImmersiveSticky() {
    getWindow()
        .getDecorView()
        .setSystemUiVisibility(
            View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                | View.SYSTEM_UI_FLAG_FULLSCREEN
                | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
  }

  public static String getIPAddress(boolean useIPv4) {
    try {
      List<NetworkInterface> interfaces = Collections.list(NetworkInterface.getNetworkInterfaces());
      for (NetworkInterface intf : interfaces) {
        List<InetAddress> addrs = Collections.list(intf.getInetAddresses());
        for (InetAddress addr : addrs) {
          if (!addr.isLoopbackAddress()) {
            String sAddr = addr.getHostAddress();
            boolean isIPv4 = sAddr.indexOf(':')<0;

            if (useIPv4) {
              if (isIPv4) 
                return sAddr;
            } else {
              if (!isIPv4) {
                int delim = sAddr.indexOf('%'); // drop ip6 zone suffix
                  return delim<0 ? sAddr.toUpperCase() : sAddr.substring(0, delim).toUpperCase();
                }
            }
          }
        }
      }
    } catch (Exception ex) { } // for now eat exceptions
    return "";
  }
  private native long nativeCreateRenderer(ClassLoader appClassLoader,
					   Context context,
					   long nativeGvrContext);
  private native void nativeDestroyRenderer(long nativeVRXRenderer);
  private native void nativeInitializeGl(long nativeVRXRenderer);
  private native long nativeDrawFrame(long nativeVRXRenderer);
  private native void nativeOnTriggerEvent(long nativeVRXRenderer);
  private native void nativeOnPause(long nativeVRXRenderer);
  private native void nativeOnResume(long nativeVRXRenderer);
  private native int nativeWMEvent(long nativeVRXRenderer, int scancode, boolean down);
}
