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

  public int getFBPtr(int retries) {
    int ptr = 0;
    while ((ptr = nativeGetFrameBufferPointer()) == 0 && --retries > 0)
      try {
	Thread.sleep(1000);
      } catch (InterruptedException e){
	return 0;
      }
    return ptr;
  }

  public int getFBPtr() {
    return getFBPtr(5);
  }

  public void launch() {
    t = new Thread(this, "Server thread");
    t.start();
  }

  public void terminate() {
    // TODO: implement me
  }

  private native int nativeRunX();
  private native int nativeGetFrameBufferPointer();
}
