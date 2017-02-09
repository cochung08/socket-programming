package clientside;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.Scanner;

public class MulticastClientThread extends Thread {

  Socket sock;

  MulticastClientThread(Socket sock) { this.sock = sock; }

  public void run() {
    try {

      // DataOutputStream out = new
      // DataOutputStream(sock.getOutputStream());
      // out.writeInt(2);

      // PrintWriter pout = new PrintWriter(sock.getOutputStream(), true);
      Scanner Sinput = new Scanner(sock.getInputStream());

      while (true) {
        String tmp = Sinput.nextLine();
        System.out.println(tmp);
      }

    } catch (Exception e) {
      // TODO Auto-generated catch block
      e.printStackTrace();
    }
  }
}
