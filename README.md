# Chat
1) Run make
2) First run serv and after that run clients
3) Every client send its message to another connected clients
4) Type "stop" in servers terminal to terminate server and type "__END" to terminate client  

To test how much user could connect:
1) Run "./tm %num_of_client"
2) It will read from file defined FILE_NAME_IN in troublemaker.c and write all chat history to FILE_NAME_OUT
3) to stop type in terminal "stop" so main process will write "__END" in FILE_NAME_IN
