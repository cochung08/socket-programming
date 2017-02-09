package severside;

import java.io.IOException;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;

public class main {

  static public MulticastUtil mUtil = new MulticastUtil();

  public static void main(String[] args) throws IOException {
    // int portNumber = Integer.parseInt(args[0]);
    // String filename = args[1];

    String address = "127.0.0.1";
    int dataPortNumber = 10003;
    int multicastPortNumber = 10004;

    ServerSocket dataservsock = new ServerSocket(dataPortNumber);
    ServerSocket multicastservsock = new ServerSocket(multicastPortNumber);

    while (true) {

      Socket sockData = dataservsock.accept();
      Socket sockMutil = multicastservsock.accept();

      PrintWriter pout = new PrintWriter(sockMutil.getOutputStream(), true);
      mUtil.addStream(pout);

      Thread t = new DataServerThread(sockData, mUtil);

      // Thread m = new MulticastServerThread(sockMutil, mUtil);
      t.start();
      // m.start();
    }
  }
}
