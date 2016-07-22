package com.towersmatrix.vrx;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.FileOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

import android.content.res.AssetManager;
import android.text.TextUtils;
import android.util.Log;

public class Unpacker {
  private static final int BUFFER_SIZE = 4096;

  private File installDir;
  private AssetManager amgr;

  public Unpacker(File dir, AssetManager mgr) {
    installDir = dir;
    amgr = mgr;
  }

  public boolean verifyInstallation() {
    if (!checkPresence("/busybox"))
      {
	Log.i("VRX", "Extracting binaries");
	try {
	  ZipInputStream zis = new ZipInputStream(amgr.open("binaries-armeabi-v7a.zip"));
	  unzip(zis);
	  zis.close();
	} catch (IOException e) {
	  Log.e("VRX", "Could not extract binaries: " + e.toString());
	  return false;
	}
      }

    Log.i("VRX", "Setting execute permissions");
    try {
      setExecutable("busybox");
    } catch (IOException e) {
      Log.e("VRX", "Could not set execute permissions: " + e.toString());
      return false;
    }

    if (!checkPresence("/usr/share/X11"))
      {
	String name = "data-1.tgz";
	Log.i("VRX", "Extracting resources");
	try {
	  copyAsset(name);
	} catch (IOException e) {
	  Log.e("VRX", "Could copy " + name + ": " + e.toString());
	  return false;
	}


	String cmd[] = {installDir + File.separator +  "busybox", "tar", "-xf", name};
	try {
	  Runtime.getRuntime().exec(cmd, null, installDir);
	  setExecutable("/usr/bin/xkbcomp");
	  setExecutable("/usr/bin/xhost");
	  setExecutable("/usr/bin/xli");
	  setExecutable("/usr/bin/xsel");
	} catch (IOException e) {
	  Log.e("VRX", "Could not untar resource data with command '"
		+ TextUtils.join(" ", cmd) + ": " + e.toString());
	  return false;
	}
      }

    return true;
  }

  private boolean checkPresence(String path) {
    String p = installDir.getPath() + path;
    File f = new File(p);
    boolean exists = f.isFile() || f.isDirectory();
    Log.v("VRX", "Checking presence of path " + path + " (" + p + "): "
	  + (exists ? "found" : "not found"));
    return exists;
  }

  private void unzip(ZipInputStream zis) throws IOException {
    String destDirectory = installDir.getPath();

    ZipEntry entry = zis.getNextEntry();
    while (entry != null) {
      String p = destDirectory + File.separator + entry.getName();
      if (!entry.isDirectory()) {
	OutputStream bos = new BufferedOutputStream(new FileOutputStream(p));
        byte[] bytesIn = new byte[BUFFER_SIZE];
        int read = 0;
        while ((read = zis.read(bytesIn)) != -1) {
            bos.write(bytesIn, 0, read);
        }
        bos.close();
      } else {
	File dir = new File(p);
	dir.mkdir();
      }
      zis.closeEntry();
      entry = zis.getNextEntry();
    }
  }

  private void setExecutable(String path) throws IOException {
    String p = installDir.getPath() + File.separator + path;
    String chmod[] = { "/system/bin/chmod", "0755", p};
    Log.i("VRX", "Set execute permission on " + path + " (" + p + ")");
    try {
      Runtime.getRuntime().exec(chmod).waitFor();
    } catch (InterruptedException e) { /* TODO: what to do? */ }
  }

  private void copyAsset(String name) throws IOException {
    copyAsset(name, "/");
  }

  private void copyAsset(String name, String to) throws IOException {
    InputStream is = new BufferedInputStream(amgr.open(name));
    OutputStream os = new BufferedOutputStream(new FileOutputStream(installDir.getPath() + File.separator + name));
    byte[] bytesIn = new byte[BUFFER_SIZE];
    int read = 0;
    while ((read = is.read(bytesIn)) != -1) {
      os.write(bytesIn, 0, read);
    }
    os.close();
  }
}
