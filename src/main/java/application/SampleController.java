package main.java.application;

import java.io.InputStream;
import java.io.*;
import java.net.HttpURLConnection;
import java.net.URL;
import java.net.URLConnection;
import java.util.HashMap;
import java.util.Map;
import java.util.ResourceBundle;

import javax.sound.sampled.AudioSystem;
import javax.sound.sampled.Clip;

import com.ibm.watson.developer_cloud.http.HttpMediaType;
import com.ibm.watson.developer_cloud.speech_to_text.v1.SpeechToText;
import com.ibm.watson.developer_cloud.speech_to_text.v1.model.RecognizeOptions;
import com.ibm.watson.developer_cloud.speech_to_text.v1.model.SpeechRecognitionResults;
import com.ibm.watson.developer_cloud.text_to_speech.v1.TextToSpeech;
import com.ibm.watson.developer_cloud.text_to_speech.v1.model.GetPronunciationOptions.Voice;
import com.ibm.watson.developer_cloud.text_to_speech.v1.model.GetVoiceOptions;
import com.ibm.watson.developer_cloud.text_to_speech.v1.model.SynthesizeOptions;
import com.ibm.watson.developer_cloud.text_to_speech.v1.util.WaveUtils;

import javafx.application.Platform;
import javafx.fxml.FXML;
import javafx.fxml.Initializable;
import javafx.scene.media.Media;
import javafx.scene.media.MediaPlayer;
import javafx.scene.text.Text;
import javafx.util.Duration;

public class SampleController implements Initializable {
	
	@FXML
	public Text response;
	
	@Override
	public void initialize(URL location, ResourceBundle resources) {
	}
	
	public void recordVoice() throws InterruptedException {
		final SoundRecorder recorder = new SoundRecorder();
		Thread stopper = new Thread(new Runnable() {
			public void run() {
				try {
					Thread.sleep(2000);
				} catch (InterruptedException ex) {
					ex.printStackTrace();
				}
				recorder.finish();
			}
		});
		stopper.start();
		recorder.start();
		processSpeech();
	}
	
	public void listenMessage() {
		try
	    {
	        Clip clip = AudioSystem.getClip();
	        clip.open(AudioSystem.getAudioInputStream(new File("data/message.wav")));
	        clip.start();
	    }
	    catch (Exception exc)
	    {
	        exc.printStackTrace(System.out);
	    }
	}
	
	public void listenResponse() {
		try
	    {
	        Clip clip = AudioSystem.getClip();
	        clip.open(AudioSystem.getAudioInputStream(new File("data/response.wav")));
	        clip.start();
	    }
	    catch (Exception exc)
	    {
	        exc.printStackTrace(System.out);
	    }
	}
	
	
	public void lightsOn() {
		sendCommand("turn the lights on");
	}
	public void lightsOff() {
		sendCommand("turn the lights off");
	}
	public void condOn() {
		sendCommand("air conditioner on");
	}
	public void condOff() {
		sendCommand("air conditioner off");
	}
	public void curtainUp() {
		sendCommand("curtains on");
	}
	public void curtainDown() {
		sendCommand("curtains off");
	}
	
	public void sendCommand(String trs) {
		try {	
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
				
                try {
                    // Make a URL to the web page
                    URL url2 = new URL("http://smartroomx.eu-gb.mybluemix.net/ugur");

                    // Get the input stream through URL Connection
                    URLConnection con = url2.openConnection();
                    InputStream is =con.getInputStream();

                    BufferedReader br = new BufferedReader(new InputStreamReader(is));

                    String line = null;
                    String message = "";
                    // read each line and write to System.out
                    while ((line = br.readLine()) != null) {
                        System.out.println(line.substring(line.indexOf(":") + 1, line.indexOf("}")));
                        message += line.substring(line.indexOf(":") + 1, line.indexOf("}"));
                    }
                    response.setText(message);
                    processResponse(message);
                    listenResponse();
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
	
	public void processSpeech(){
		try {
			SpeechToText service = new SpeechToText();
			service.setUsernameAndPassword("c26521b4-64ef-4a9a-b632-26c0de1105bf", "EWgpz7Kkkpoz");
			String filePath = new java.io.File("").getAbsolutePath() + "/data/message.wav";
			filePath = filePath.replace("\\", "/");
			File audio = new File(filePath);
			
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
			sendCommand(trs);
		}
			catch(Exception e){
				System.out.println(e);
				System.out.println("Speech to text exception");
			}
		
	}
	
	public void processResponse(String message) {
		TextToSpeech textToSpeech = new TextToSpeech();
		textToSpeech.setUsernameAndPassword("897a5521-a801-482b-8f03-2e69f52dfc32", "AknvSHVzwo1B");
		textToSpeech.setEndPoint("https://stream.watsonplatform.net/text-to-speech/api");
		try {
		  SynthesizeOptions synthesizeOptions =
		    new SynthesizeOptions.Builder()
		      .text(message)
		      .accept("audio/wav")
		      .voice("en-US_MichaelVoice")
		      .build();

		  InputStream inputStream =
		    textToSpeech.synthesize(synthesizeOptions).execute();
		  InputStream in = WaveUtils.reWriteWaveHeader(inputStream);

		  OutputStream out = new FileOutputStream("data/response.wav");
		  byte[] buffer = new byte[1024];
		  int length;
		  while ((length = in.read(buffer)) > 0) {
		    out.write(buffer, 0, length);
		  }
		  
		  out.close();
		  in.close();
		  inputStream.close();
		} catch (IOException e) {
		  e.printStackTrace();
		}
	}

	
}
