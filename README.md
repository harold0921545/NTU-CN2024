# NTU-CN2024
Computer Network project - a real-time online chatroom.
Currently, we are in Phase 1. Phase 2 will be made public once the project deadline is reached.

## Compilation
```sh
cd code/
make
```

## Usage

### server

```sh
./server <port number>
```
### client

```sh
./client <server ip> <port number>
```
1. **Connect to the Server**
   - Once connected, you will be prompted to choose one of two options:
     - **1. Register**: Create a new account by providing a `username` and `password`.
     - **2. Login**: Log in with an existing account using your `username` and `password`.

   - Enter a single digit (e.g., `1` or `2`) to select your choice.

2. **After Login**
   - Upon successful login, you can choose between the following options:
     - **1. Send Message**: Enter a line of text that you want to send as a message.
     - **2. Logout**: End the session and exit the program.

   - Again, enter a single digit (e.g., `1` or `2`) to make your choice.

## Example Flow

1. Connect to the server.
2. Enter `1` to Register or `2` to Login.
   - If Register: Provide `username` and `password`.
   - If Login: Provide `username` and `password`.
3. After Login:
   - Enter `1` to send a message. Type your message and press Enter.
   - Enter `2` to logout and close the program.

### Notes

- Input should be a single digit without any punctuation (e.g., just `1` or `2`).
- After logging out, the program will terminate.
