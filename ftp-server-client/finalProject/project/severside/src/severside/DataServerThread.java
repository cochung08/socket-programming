package severside;

import java.io.BufferedInputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectOutputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.ServerSocket;
import java.net.Socket;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Vector;

public class DataServerThread extends Thread {

  String directoryName = "dataFile";
  String filePath = "dataFile\\";

  Socket sock;
  MulticastUtil mUtil;

  public DataServerThread(Socket sock, MulticastUtil mUtil) throws IOException {

    this.sock = sock;
    this.mUtil = mUtil;
  }

  public void run() {

    try {

      InputStream is = sock.getInputStream();
      OutputStream os = sock.getOutputStream();

      DataInputStream in = new DataInputStream(is);
      DataOutputStream out = new DataOutputStream(os);

      while (true) {

        String request = in.readUTF();
        System.out.println(request);

        if (request.startsWith("ls")) {

          list(sock, directoryName);

        }

        else if (request.startsWith("delete"))

        {
          request = request.substring(7);
          String fileName = filePath + request;
          File myFile = new File(fileName);
          myFile.delete();

          Thread MulticastServerThread =
              new MulticastServerThread(mUtil, "File Deleted: " + request);
          MulticastServerThread.start();

        }

        else if (request.startsWith("upload"))

        {
          String fileName = upload(sock);

          Thread MulticastServerThread =
              new MulticastServerThread(mUtil, "File uploaded: " + fileName);
          MulticastServerThread.start();

        }

        else if (request.startsWith("download")) {

          request = request.substring(9);
          String filename = filePath + request;

          download(sock, filename);
        }
      }

    } catch (IOException e) {
      // TODO Auto-generated catch block
      e.printStackTrace();
    }
  }

  private String upload(Socket sock) throws IOException {
    DataInputStream inU = new DataInputStream(sock.getInputStream());
    int size = inU.readInt();

    String Ufname = inU.readUTF();

    Ufname = getName(Ufname);

    String fullpath = filePath + "\\" + Ufname;
    FileOutputStream fos = new FileOutputStream(fullpath);

    System.out.println("Server says " + size);

    byte[] buffer = new byte[size];
    inU.readFully(buffer, 0, size);
    fos.write(buffer, 0, size);
    fos.close();

    return Ufname;
  }

  private void download(Socket sock, String filename)
      throws FileNotFoundException, IOException {
    File myFile = new File(filename);
    byte[] mybytearray = new byte[(int)myFile.length()];

    FileInputStream fis = new FileInputStream(myFile);
    BufferedInputStream bis = new BufferedInputStream(fis);

    DataInputStream dis = new DataInputStream(bis);
    dis.readFully(mybytearray, 0, mybytearray.length);

    DataOutputStream outD = new DataOutputStream(sock.getOutputStream());

    outD.writeInt(mybytearray.length);
    outD.write(mybytearray, 0, mybytearray.length);
    outD.flush();
  }

  private void list(Socket sock, String directoryName) throws IOException {
    ObjectOutputStream oos = new ObjectOutputStream(sock.getOutputStream());

    File directory = new File(directoryName);
    File[] fList = directory.listFiles();
    oos.writeObject(fList);
  }

  private static String getName(String Dfname) {
    String filePath = "dataFile";

    File directory = new File(filePath);
    File[] fList = directory.listFiles();

    int count = 2;
    for (int i = 0; i < fList.length; i++) {
      if (fList[i].getName().equals(Dfname)) {

        int l = Dfname.indexOf('[');
        if (l < 0) {
          int m = Dfname.indexOf('.');
          String left = Dfname.substring(0, m);
          String right = Dfname.substring(m + 1);

          Dfname = left + "[1]." + right;
        }

        else {

          int r = Dfname.indexOf(']');

          String left = Dfname.substring(0, l + 1);
          String right = Dfname.substring(r);

          Dfname = left + count + right;
          count++;
          i = 0;
          fList = directory.listFiles();
        }
      }
    }
    return Dfname;
  }
}