package severside;

import java.io.PrintWriter;
import java.util.Vector;

class MulticastUtil {
  private Vector<PrintWriter> outputStreamList;

  public MulticastUtil() { outputStreamList = new Vector(); }

  synchronized void addStream(PrintWriter outStream) {
    if (!(outputStreamList.contains(outStream))) {
      outputStreamList.addElement(outStream);
      System.out.println("Connect with new client ");
    }
  }

  synchronized void sendMessageToStreams(String s) {

    System.out.println("**************************************\n"
                       + "Broadcast initiated ---");

    for (int i = 0; i < outputStreamList.size(); i++) {
      System.out.println("sending to client" + i);

      PrintWriter outStream = (PrintWriter)outputStreamList.elementAt(i);

      outStream.println(s);
    }

    System.out.println("********************************\n"
                       + "Server completed Broadcast ---");
  }

  synchronized void removeStream(PrintWriter outStream) {

    if (outputStreamList.removeElement(outStream)) {
      System.out.println("disconnected client ");
    } else {
      System.out.println("Unregister: client wasn't registered.");
    }
  }
}