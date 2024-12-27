# NTU-CN2024

**Computer Network Project** - A Real-Time Online Chatroom

This project implements a real-time chatroom application that supports:
- User registration and login
- Text-based communication
- File transfer
- Audio file streaming and playback

## Prerequisites

To ensure the application runs correctly, install the following dependencies:

```sh
sudo apt-get install openssl
sudo apt-get install libsdl2-dev
sudo apt-get install pulseaudio
```

Additionally, generate the required SSL certificates for secure communication. In the server directory, run the following command:

```sh
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -sha256 -days 365
```
- Place the generated cert.pem file in the client directory.

## Compilation

Navigate to the `code/` directory and build the project using `make`:

```sh
cd code/
make
```

## Usage

### Starting the Server

Run the server with the following syntax:

```sh
./server <connection port number> <chatting port number> <file port number> <audio port number>
```

### Starting the Client

Run the client with the following syntax:

```sh
./client <server ip> <connection port number> <chatting port number> <file port number> <audio port number>
```

## Features

### 1. **Connecting to the Server**
   After launching the client, connect to the server. You will be prompted with three options:

   - **1. Register**: Create a new account by entering a `username` and `password`.
   - **2. Login**: Access an existing account using your `username` and `password`.
   - **3. Logout**: Disconnect from the server and exit the program.

   Enter a single digit (e.g., `1`, `2`, or `3`) to make your choice.

### 2. **After Login**
   Upon successful login, you will have access to the following functionalities:

   #### **1. Texting**
   - Enter the recipient's name to open a chatroom and start messaging.

   #### **2. Transfer File**
   - Select this option to send files to another user.

   #### **3. Receive File**
   - Select this option to receive files sent by other users.

   #### **4. Transfer Audio File**
   - Stream an audio file to another user in real-time.

   #### **5. Playback the Received Audio**
   - Play back an audio file received from another user, it will not terminate until the end of the audio.

   #### **6. Logout**
   - End the session and exit the program.

### Notes

- Input for menu options should be a single digit (e.g., `1`, `2`, etc.).
- Only .wav format is supported while tranfer audio file.
- A sample Audio.wav file is available in the client directory for testing.

This project demonstrates the fundamental concepts of computer networking and multimedia streaming using tools like OpenSSL, SDL2, and PulseAudio.

