package clientside;

import java.net.Socket;

public class main {
  public static void main(String[] argv) throws Exception {

    // String adress = argv[0];
    // int portNumber = Integer.parseInt(argv[1]);
    // String filename = argv[2];
    String adress = "127.0.0.1";
    int dataPortNumber = 10003;
    int multicastPortNumber = 10004;

    Socket sockData = new Socket(adress, dataPortNumber);
    Socket sockMutil = new Socket(adress, multicastPortNumber);

    System.out.println("data" + sockData);
    System.out.println("cast" + sockMutil);

    Thread t = new DataClientThread(sockData);
    t.start();

    Thread n = new MulticastClientThread(sockMutil);
    n.start();
  }
}
