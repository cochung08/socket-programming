package severside;

import java.io.IOException;
import java.io.PrintWriter;
import java.net.Socket;

public class MulticastServerThread extends Thread {

  MulticastUtil mUtil;
  String msg;

  MulticastServerThread(MulticastUtil mUtil, String msg) {
    this.msg = msg;
    this.mUtil = mUtil;
  }

  public void run() { mUtil.sendMessageToStreams(msg); }
}
