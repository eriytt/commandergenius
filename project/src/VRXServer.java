package com.towersmatrix.vrx.xserver;

public class VRXServer implements Runnable{
  static {
    System.loadLibrary("application");
  }

  private Thread t = null;

  public VRXServer() {
    super();
  }

  public void run() {
    int exitcode = nativeRunX();
  }

  public void launch() {
    t = new Thread(this, "Server thread");
    t.start();
  }

  public void terminate() {
    // TODO: implement me
  }

  private native int nativeRunX();
}
