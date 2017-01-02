package com.towersmatrix.vrx.xserver;

public class VRXServer implements Runnable{
  static {
    System.loadLibrary("application");
  }

  private Thread t = null;
  String mFilesDirectory;

  public VRXServer(String filesDirectory) {
    super();
    mFilesDirectory = filesDirectory;
  }

  public void run() {
    int exitcode = nativeRunX(mFilesDirectory);
  }

  public void launch() {
    t = new Thread(this, "Server thread");
    t.start();
  }

  public void terminate() {
    // TODO: implement me
  }

  private native int nativeRunX(String filesDirectory);
  public native void nativeKeyEvent(int scancode, boolean down);
  public native void nativeMouseMotionEvent(int x, int y);
}
