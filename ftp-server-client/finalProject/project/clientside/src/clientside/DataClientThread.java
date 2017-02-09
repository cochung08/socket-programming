package clientside;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.ObjectInputStream;
import java.io.OutputStream;
import java.io.PrintWriter;
import java.net.Socket;
import java.util.Scanner;

public class DataClientThread extends Thread {

  Socket sock;

  public DataClientThread(Socket sock) throws IOException { this.sock = sock; }

  public void run()

  {

    String filePath = "dataFile";

    while (true) {

      System.out.println("please enter you command");
      Scanner reader = new Scanner(System.in);

      String request = reader.nextLine();
      try {
        if (request.equals("ls")) {
          list(sock, request);
        }

        else if (request.startsWith("upload")) {
          upload(sock, filePath, request);

        }

        else if (request.startsWith("download")) {

          download(sock, filePath, request);

        } else if (request.startsWith("delete")) {
          delete(sock, request);
        }

      } catch (Exception e) {
        // TODO Auto-generated catch block
        e.printStackTrace();
      }
    }
  }

  private static void delete(Socket sock, String request)
      throws IOException, ClassNotFoundException {

    DataOutputStream outD = new DataOutputStream(sock.getOutputStream());

    outD.writeUTF(request);
  }

  private static void list(Socket sock, String request)
      throws IOException, ClassNotFoundException {
    DataOutputStream outD = new DataOutputStream(sock.getOutputStream());

    outD.writeUTF(request);
    ObjectInputStream ois = new ObjectInputStream(sock.getInputStream());

    File[] fList = (File[])ois.readObject();
    for (File file : fList) {
      System.out.println(file.getName());
    }
  }

  private static void download(Socket sock, String filePath, String request)
      throws IOException, FileNotFoundException {
    String Dfname = request.substring(9);

    Dfname = getName(Dfname);

    DataInputStream inD = new DataInputStream(sock.getInputStream());

    DataOutputStream outD = new DataOutputStream(sock.getOutputStream());

    outD.writeUTF(request);

    String fullpath = filePath + "\\" + Dfname;

    FileOutputStream fos = new FileOutputStream(fullpath);

    int size = inD.readInt();
    System.out.println("Server says " + size);

    byte[] buffer = new byte[size];
    inD.readFully(buffer, 0, size);
    fos.write(buffer, 0, size);
    fos.close();

    // byte[] buffer = new byte[1024];
    // while (size > 0 && (bytesRead = in.read(buffer, 0, (int)
    // Math.min(buffer.length, size))) != -1) {
    // fos.write(buffer, 0, bytesRead);
    // size -= bytesRead;
    // }

    // byte[] buffer = new byte[size];
    // while (size > 0 && (bytesRead = in.read(buffer, 0,size)) != -1) {
    // System.out.println(bytesRead);
    // fos.write(buffer, 0, bytesRead);
    // size -= bytesRead;
    // }
  }

  private static void upload(Socket sock, String filePath, String request)
      throws FileNotFoundException, IOException {
    String Dfname = request.substring(7);
    String filename = filePath + "\\" + Dfname;

    File myFile = new File(filename);
    byte[] mybytearray = new byte[(int)myFile.length()];

    FileInputStream fis = new FileInputStream(myFile);
    BufferedInputStream bis = new BufferedInputStream(fis);

    DataInputStream dis = new DataInputStream(bis);
    dis.readFully(mybytearray, 0, mybytearray.length);

    DataOutputStream outD = new DataOutputStream(sock.getOutputStream());

    outD.writeUTF(request);
    outD.writeInt(mybytearray.length);
    outD.writeUTF(Dfname);
    outD.write(mybytearray, 0, mybytearray.length);
    outD.flush();
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
