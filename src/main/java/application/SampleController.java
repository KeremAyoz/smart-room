package application;

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.*;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;

//import javax.sound.*;
import javax.sound.sampled.AudioFileFormat;
import javax.sound.sampled.AudioFormat;
import javax.sound.sampled.AudioInputStream;
import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.DataLine;
import javax.sound.sampled.LineUnavailableException;
import javax.sound.sampled.SourceDataLine;
import javax.sound.sampled.TargetDataLine;
import javax.sound.sampled.UnsupportedAudioFileException;

import com.ibm.watson.developer_cloud.http.HttpMediaType;
import com.ibm.watson.developer_cloud.speech_to_text.v1.SpeechToText;
import com.ibm.watson.developer_cloud.speech_to_text.v1.model.RecognizeOptions;
import com.ibm.watson.developer_cloud.speech_to_text.v1.model.SpeechRecognitionResults;
import org.json.simple.JSONObject;
import org.omg.CORBA.portable.OutputStream;

import javafx.scene.media.Media;
import javafx.scene.media.MediaPlayer;
import javafx.util.Duration;

import java.net.URI;
import java.util.Scanner;


public class SampleController {

	public void recordVoice2() throws UnsupportedAudioFileException, IOException {

		AudioFormat format = new AudioFormat(8000.0f, 16, 1, true, true);
		TargetDataLine microphone;
		AudioInputStream audioInputStream;
		SourceDataLine sourceDataLine;
		try {
			microphone = AudioSystem.getTargetDataLine(format);

			DataLine.Info info = new DataLine.Info(TargetDataLine.class, format);
			microphone = (TargetDataLine) AudioSystem.getLine(info);
			microphone.open(format);

			ByteArrayOutputStream out = new ByteArrayOutputStream();
			int numBytesRead;
			int CHUNK_SIZE = 1024;
			byte[] data = new byte[microphone.getBufferSize() / 5];
			microphone.start();

			int bytesRead = 0;

			try {
				while (bytesRead < 100000) { // Just so I can test if recording
												// my mic works...
					numBytesRead = microphone.read(data, 0, CHUNK_SIZE);
					bytesRead = bytesRead + numBytesRead;
					System.out.println(bytesRead);
					out.write(data, 0, numBytesRead);
				}
			} catch (Exception e) {
				e.printStackTrace();
			}
			byte audioData[] = out.toByteArray();
			// Get an input stream on the byte array
			// containing the data
			InputStream byteArrayInputStream = new ByteArrayInputStream(audioData);
			audioInputStream = new AudioInputStream(byteArrayInputStream, format, audioData.length / format.getFrameSize());
			DataLine.Info dataLineInfo = new DataLine.Info(SourceDataLine.class, format);
			sourceDataLine = (SourceDataLine) AudioSystem.getLine(dataLineInfo);
			sourceDataLine.open(format);
			sourceDataLine.start();
			int cnt = 0;
			byte tempBuffer[] = new byte[10000];
			
			
			
			try {
				while ((cnt = audioInputStream.read(tempBuffer, 0, tempBuffer.length)) != -1) {
					if (cnt > 0) {
						// Write data to the internal buffer of
						// the data line where it will be
						// delivered to the speaker.
						sourceDataLine.write(tempBuffer, 0, cnt);
					} // end if
				}
			} catch (IOException e) {
				e.printStackTrace();
			}
			
			InputStream bais = new ByteArrayInputStream(tempBuffer);
			AudioInputStream ais = AudioSystem.getAudioInputStream(bais);
			FileOutputStream os = new FileOutputStream(new File("sound.mp3"));
			int k = AudioSystem.write(ais, AudioFileFormat.Type.WAVE, os);
			
			
			// Block and wait for internal buffer of the
			// data line to empty.
			sourceDataLine.drain();
			sourceDataLine.close();
			microphone.close();
		} catch (LineUnavailableException e) {
			e.printStackTrace();
		}
	}

	
	public void recordVoice() {
		final SoundRecorder recorder = new SoundRecorder();

		// creates a new thread that waits for a specified
		// of time before stopping
		Thread stopper = new Thread(new Runnable() {
			public void run() {
				try {
					Thread.sleep(4000);
				} catch (InterruptedException ex) {
					ex.printStackTrace();
				}
				recorder.finish();
			}
		});

		stopper.start();

		// start recording
		recorder.start();
	}
	
	public void listenVoice() {
		{
			final MediaPlayer musicplayer;
			String filePath = "file:///" + new java.io.File("").getAbsolutePath() + "/data/RecordAudio.wav";
			filePath = filePath.replace("\\", "/");
			Media mp3MusicFile = new Media(filePath);
			musicplayer = new MediaPlayer(mp3MusicFile);
			musicplayer.setAutoPlay(true);
			musicplayer.setVolume(0.5);
			musicplayer.setOnEndOfMedia(new Runnable() {
				public void run() {
					musicplayer.seek(Duration.ZERO);
				}
			});
		}
	}

	public void sendCommand(){
		try {
			SpeechToText service = new SpeechToText();
			service.setUsernameAndPassword("c26521b4-64ef-4a9a-b632-26c0de1105bf", "EWgpz7Kkkpoz");
			String filePath = new java.io.File("").getAbsolutePath() + "/data/RecordAudio.wav";
			filePath = filePath.replace("\\", "/");
			File audio = new File(filePath);

			//see http://www.ibm.com/smarterplanet/us/en/ibmwatson/developercloud/apis/#!/speech-to-text/recognizeSession
			//for parameter explanation

			Map<String, Object> params = new HashMap<String, Object>();

			RecognizeOptions options = new RecognizeOptions.Builder()
					.audio(audio)
					.contentType(HttpMediaType.AUDIO_WAV)
					.build();

			SpeechRecognitionResults transcript = service.recognize(options).execute();
			System.out.println(transcript.getResults());
			String trs = transcript.toString();
			if(trs.length() == 0){
			    System.out.println("Cannot receive command");
			    return;
            }
			trs = trs.substring((trs.indexOf("transcript") + 12));
			trs = trs.substring(0, trs.indexOf(","));
			trs= trs.substring(trs.indexOf("\"") + 1, trs.lastIndexOf("\""));

			try {
				String urlParameters = "message=" + trs;
				String request = "https://smartroomx.eu-gb.mybluemix.net/sound";
				URL url = new URL(request);

				HttpURLConnection connection = (HttpURLConnection) url.openConnection();
				connection.setDoOutput(true);
				connection.setDoInput(true);
				connection.setInstanceFollowRedirects(false);
				connection.setRequestMethod("POST");
				connection.setRequestProperty("Content-Type","application/x-www-form-urlencoded");
				connection.setRequestProperty("charset", "utf-8");
				connection.setRequestProperty("Content-Length","" + Integer.toString(urlParameters.getBytes().length));
				connection.setUseCaches(false);
				System.out.println("Connected to the url");
				DataOutputStream wr = new DataOutputStream(connection.getOutputStream());
				System.out.println("Sending message");
				wr.writeBytes(urlParameters);
				System.out.println("Sent");
				int code = connection.getResponseCode();
				if(code == 200)
					System.out.println("Success with the response code : " + code);
				else
					System.out.println("Response code : " + code);
				wr.flush();
				wr.close();
				connection.disconnect();

                String content = null;
                URLConnection connection2 = null;
                try {
                    // Make a URL to the web page
                    URL url2 = new URL("http://smartroomx.eu-gb.mybluemix.net/ugur");

                    // Get the input stream through URL Connection
                    URLConnection con = url2.openConnection();
                    InputStream is =con.getInputStream();

                    BufferedReader br = new BufferedReader(new InputStreamReader(is));

                    String line = null;

                    // read each line and write to System.out
                    while ((line = br.readLine()) != null) {
                        System.out.println(line.substring(line.indexOf(":") + 1, line.indexOf("}")));
                    }
                }catch ( Exception ex ) {
                    ex.printStackTrace();
                }
            }
			catch(Exception e){
				System.out.println(e);
				System.out.println("Post exception");
			}
		}
		catch(Exception e){
			System.out.println(e);
			System.out.println("Speech to text exception");
		}
	}

}
