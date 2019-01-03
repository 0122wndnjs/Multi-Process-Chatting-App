 #include <stdio.h>
 #include <fcntl.h>
 #include <errno.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/wait.h>
 #include "comm.h"
 #include "util.h"

 /* -----------Functions that implement server functionality -------------------------*/

 /*
  * Returns the empty slot on success, or -1 on failure
  */
 int find_empty_slot(USER * user_list) {
 	// iterate through the user_list and check m_status to see if any slot is EMPTY
 	// return the index of the empty slot
     int i = 0;
 	for(i=0;i<MAX_USER;i++) {
     	if(user_list[i].m_status == SLOT_EMPTY) {
 			return i;
 		}
 	}
 	return -1;
 }

 /*
  * list the existing users on the server shell
  */
 int list_users(int idx, USER * user_list)
 {
 	// iterate through the user list
 	// if you find any slot which is not empty, print that m_user_id
 	// if every slot is empty, print "<no users>""
 	// If the function is called by the server (that is, idx is -1), then printf the list
 	// If the function is called by the user, then send the list to the user using write() and passing m_fd_to_user
 	// return 0 on success
 	int i, flag = 0;
 	char buf[MAX_MSG] = {}, *s = NULL;

 	/* construct a list of user names */
 	s = buf;
 	strncpy(s, "---connecetd user list---\n", strlen("---connecetd user list---\n"));
 	s += strlen("---connecetd user list---\n");
 	for (i = 0; i < MAX_USER; i++) {
 		if (user_list[i].m_status == SLOT_EMPTY)
 			continue;
 		flag = 1;
 		strncpy(s, user_list[i].m_user_id, strlen(user_list[i].m_user_id));
 		s = s + strlen(user_list[i].m_user_id);
 		strncpy(s, "\n", 1);
 		s++;
 	}
 	if (flag == 0) {
 		strcpy(buf, "<no users>\n");
 	} else {
 		s--;
 		strncpy(s, "\0", 1);
 	}

 	if(idx < 0) {
 		printf(buf);
 		printf("\n");
 	} else {
 		/* write to the given pipe fd */
 		if (write(user_list[idx].m_fd_to_user, buf, strlen(buf) + 1) < 0)
 			perror("writing to server shell");
 	}

 	return 0;
 }

 /*
  * add a new user
  */
 int add_user(int idx, USER * user_list, int pid, char * user_id, int pipe_to_child, int pipe_to_parent)
 {
 	// populate the user_list structure with the arguments passed to this function
 	// return the index of user added

  if (find_user_index(user_list, user_id) != -1) {
    kill(pid, SIGTERM);
    return -1;
  }
    user_list[idx].m_pid = pid;
    strcpy(user_list[idx].m_user_id, user_id);
    user_list[idx].m_fd_to_user = pipe_to_child;
    user_list[idx].m_fd_to_server = pipe_to_parent;
    user_list[idx].m_status = SLOT_FULL;

    return idx;
 }

 /*
  * Kill a user
  */
 void kill_user(int idx, USER * user_list) {
 	// kill a user (specified by idx) by using the systemcall kill()
   kill(user_list[idx].m_pid, SIGTERM); //terminate process; send terminate signal
 	// then call waitpid on the user
   waitpid(user_list[idx].m_pid, &user_list[idx].m_status, 0); //need to set 3rd arg //waitpid(pid_t pid, int *status, int options);
 }

 /*
  * Perform cleanup actions after the used has been killed
  */
 void cleanup_user(int idx, USER * user_list)
 {
 	// m_pid should be set back to -1
   user_list[idx].m_pid = -1;
 	// m_user_id should be set to zero, using memset()
   memset(user_list[idx].m_user_id, '\0', MAX_USER_ID);
 	// close all the fd
   close(user_list[idx].m_fd_to_user);
   close(user_list[idx].m_fd_to_server);
 	// set the value of all fd back to -1
   user_list[idx].m_fd_to_user = -1;
   user_list[idx].m_fd_to_server = -1;
 	// set the status back to empty
   user_list[idx].m_status = SLOT_EMPTY;
 }

 /*
  * Kills the user and performs cleanup
  */
 void kick_user(int idx, USER * user_list) {
 	// should kill_user()
   kill_user(idx, user_list);
 	// then perform cleanup_user()
   cleanup_user(idx, user_list);
 }

 /*
  * broadcast message to all users
  */
 int broadcast_msg(USER * user_list, char *buf, char *sender)
 {
   char buffer[MAX_MSG];
   char notification[MAX_MSG];
   char send[MAX_MSG];
   char colon[MAX_MSG];

   memset(buffer, '\0', sizeof(buffer));
   memset(notification, '\0', sizeof(buffer));
   memset(send, '\0', sizeof(buffer));
   memset(colon, '\0', sizeof(buffer));
   strcpy(buffer, buf);
   strcpy(send, sender);

   if(strcmp(send, "admin") == 0) {
     strcpy(notification, "admin notice: ");
     strcpy(buffer, strcat(notification, buffer));
   } else {
     strcpy(colon, ":");
     strcat(send, colon);
     strcpy(buffer, strcat(send, buffer));
   }

  // int pipe_SERVER_writing_to_child[2];
 	//iterate over the user_list and if a slot is full, and the user is not the sender itself,
   for(int i = 0; i < MAX_USER; i++){
     if (user_list[i].m_status == SLOT_FULL && strcmp(user_list[i].m_user_id, sender) != 0) {
       //then send the message to that user
       write(user_list[i].m_fd_to_user, buffer, sizeof(buffer)); //***CHECK THIS***
     }
   }

   for (int n = 0; n < MAX_USER; n++) {
    if (user_list[n].m_status == SLOT_FULL && strcmp(user_list[n].m_user_id, sender) > 0) {
      write(user_list[n].m_fd_to_user, buf, sizeof(buf));
    }
  }
  memset(buf, '\0', sizeof(buf));

  return 0;
 }

 /*
  * Cleanup user chat boxes
  */
 void cleanup_users(USER * user_list)
 {
 	// go over the user list and check for any empty slots
  for (int i = 0; i < MAX_USER; i++) {
    if(user_list[i].m_status == SLOT_EMPTY) {
      continue;
      // call cleanup user for each of those users.
      cleanup_user(i, user_list);
    }
  }

 }

 /*
  * find user index for given user name
  */
 int find_user_index(USER * user_list, char * user_id)
 {
 	// go over the  user list to return the index of the user which matches the argument user_id
 	// return -1 if not found
 	int i, user_idx = -1;

 	if (user_id == NULL) {
 		fprintf(stderr, "NULL name passed.\n");
 		return user_idx;
 	}
 	for (i=0;i<MAX_USER;i++) {
 		if (user_list[i].m_status == SLOT_EMPTY)
 			continue;
 		if (strcmp(user_list[i].m_user_id, user_id) == 0) {
 			return i;
 		}
 	}

 	return -1;
 }

 /*
  * given a command's input buffer, extract name
  */
 int extract_name(char * buf, char * user_name)
 {
 	char inbuf[MAX_MSG];
     char * tokens[16];
     strcpy(inbuf, buf);

     int token_cnt = parse_line(inbuf, tokens, " ");

     if(token_cnt >= 2) {
         strcpy(user_name, tokens[1]);
         return 0;
     }

     return -1;
 }

 int extract_text(char *buf, char * text)
 {
     char inbuf[MAX_MSG];
     char * tokens[16];
     char * s = NULL;
     strcpy(inbuf, buf);

     int token_cnt = parse_line(inbuf, tokens, " ");

     if(token_cnt >= 3) {
         //Find " "
         s = strchr(buf, ' ');
         s = strchr(s+1, ' ');

         strcpy(text, s+1);
         return 0;
     }

     return -1;
 }

 /*
  * send personal message
  */
 void send_p2p_msg(int idx, USER * user_list, char *buf)
 {
  char user_name[MAX_USER_ID];
  char not_found[MAX_MSG];
  char text[MAX_MSG];
  char col[MAX_MSG];
  char message[MAX_MSG];

  memset(user_name, '\0', sizeof(user_name));
  memset(not_found, '\0', sizeof(not_found));
  memset(text, '\0', sizeof(text));
  memset(col, '\0', sizeof(col));
  memset(message, '\0', sizeof(message));

  if(extract_name(buf, user_name) == 0) {
    if (find_user_index(user_list, user_name) == -1) {
      strcpy(not_found, "User not found");
      write(user_list[idx].m_fd_to_user, not_found, sizeof(not_found));
      memset(buf, '\0', sizeof(buf));
    } else {
      extract_text(buf, text);
      strcpy(col, ":");
      strcpy(message, user_list[idx].m_user_id);
      strcat(message, col);
      strcat(message, text);

      write(user_list[find_user_index(user_list, user_name)].m_fd_to_user, message, sizeof(message));
      memset(buf, '\0', sizeof(buf));
    }
  } else {
    strcpy(not_found, "User is not given");
    write(user_list[idx].m_fd_to_user, not_found, sizeof(not_found));
    memset(buf, '\0', sizeof(buf));
  }
 }

 //takes in the filename of the file being executed, and prints an error message stating the commands and their usage
 void show_error_message(char *filename)
 {
   perror("ERROR!");
 }


 /*
  * Populates the user list initially
  */
 void init_user_list(USER * user_list) {

 	//iterate over the MAX_USER
 	//memset() all m_user_id to zero
 	//set all fd to -1
 	//set the status to be EMPTY
 	int i=0;
 	for(i=0;i<MAX_USER;i++) {
 		user_list[i].m_pid = -1;
 		memset(user_list[i].m_user_id, '\0', MAX_USER_ID);
 		user_list[i].m_fd_to_user = -1;
 		user_list[i].m_fd_to_server = -1;
 		user_list[i].m_status = SLOT_EMPTY;
 	}
 }

/* ---------------------End of the functions that implementServer functionality -----------------*/


/* ---------------------Start of the Main function ----------------------------------------------*/
int main(int argc, char * argv[])
{
	int nbytes;
	setup_connection("STH"); // Specifies the connection point as argument.

	USER user_list[MAX_USER];
	init_user_list(user_list);   // Initialize user list

	char buf[MAX_MSG];
	fcntl(0, F_SETFL, fcntl(0, F_GETFL)| O_NONBLOCK);
	print_prompt("admin");
  char * user_id[MAX_USER_ID];
  int is_connected;

	while(1) {
		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/

		// Handling a new connection using get_connection
		int pipe_SERVER_reading_from_child[2];
		int pipe_SERVER_writing_to_child[2];
    int pipe_child_reading_from_user[2];
    int pipe_child_writing_to_user[2];
		char user_id[MAX_USER_ID];

		// Check max user and same user id

		// Child process: poli users and SERVER

		// Server process: Add a new user information into an empty slot
		// poll child processes and handle user commands
		// Poll stdin (input from the terminal) and handle admnistrative command

    memset(buf, '\0', sizeof(buf));

    if(get_connection(user_id, pipe_child_writing_to_user, pipe_child_reading_from_user) == 0) {
      is_connected = 1;
      pipe(pipe_SERVER_reading_from_child);
      pipe(pipe_SERVER_writing_to_child);
      fcntl(pipe_SERVER_reading_from_child[0], F_SETFL, O_NONBLOCK);
      fcntl(pipe_SERVER_writing_to_child[0], F_SETFL, O_NONBLOCK);
      fcntl(pipe_child_reading_from_user[0], F_SETFL, O_NONBLOCK);
      fcntl(pipe_child_writing_to_user[0], F_SETFL, O_NONBLOCK);
      fcntl(0, F_SETFL, O_NONBLOCK);

      pid_t pid = fork();

      if(pid == 0) { // Child
        close(pipe_SERVER_reading_from_child[0]);
        close(pipe_SERVER_writing_to_child[1]);

        while(1) {
          memset(buf, '\0', sizeof(buf));
          int b = read(pipe_child_reading_from_user[0], buf, MAX_MSG);

          if (b > 0) {
            write(pipe_SERVER_reading_from_child[1], buf, sizeof(buf));
            memset(buf, '\0', sizeof(buf));
          }
          if (b == 0) {
            write(pipe_SERVER_reading_from_child[1], buf, sizeof(buf));
            close(pipe_child_writing_to_user[1]);
            close(pipe_child_reading_from_user[0]);
            close(pipe_SERVER_reading_from_child[1]);
            close(pipe_SERVER_writing_to_child[0]);
            exit(0);
          }
          if(read(pipe_SERVER_writing_to_child[0], buf, MAX_MSG) > 0) {
            write(pipe_child_writing_to_user[1], buf, sizeof(buf));
            memset(buf, '\0', sizeof(buf));
          }
          if(read(pipe_SERVER_writing_to_child[0], buf, MAX_MSG) == 0) {
            close(pipe_child_writing_to_user[1]);
            close(pipe_child_reading_from_user[0]);
            close(pipe_SERVER_reading_from_child[1]);
            close(pipe_SERVER_writing_to_child[0]);
            exit(0);
          }
          memset(buf, '\0', sizeof(buf));
          usleep(50);
        }
      } else if(pid > 0) { // Parent
        int empty_slot_idx = find_empty_slot(user_list);
        if(empty_slot_idx == -1) {
          printf("\nNo more slots\n");
          // when user is more than 10, show this message
          print_prompt("admin");
          strcpy(buf, "The server process seems dead");
          // and shows the message
          write(pipe_child_writing_to_user[1], buf, sizeof(buf));
          kill(pid, SIGKILL);
          // than terminate the 11th user
          memset(buf, '\0', sizeof(buf));
        } else {
          memset(buf, '\0', sizeof(buf));
          extract_name(buf, user_id);
          if(add_user(empty_slot_idx, user_list, pid, user_id,
            pipe_SERVER_writing_to_child[1], pipe_SERVER_reading_from_child[0]) == -1) {
              printf("User id %s is already used\n", user_id);
              // when trying to use same user ID, show this message
              print_prompt("admin");
              kill(pid, SIGTERM);
              // And than terminate the client
              memset(buf, '\0', sizeof(buf));
            } else {
              printf("\nA new user %s is connected, slot: %d\n",
                                                      user_id, empty_slot_idx);
              print_prompt("admin");
              memset(buf, '\0', sizeof(buf));
            }

            close(pipe_child_reading_from_user[0]);
            close(pipe_child_reading_from_user[1]);
            close(pipe_child_writing_to_user[0]);
            close(pipe_child_writing_to_user[1]);
            close(pipe_SERVER_reading_from_child[1]);
            close(pipe_SERVER_writing_to_child[0]);
          }
      } else {
        perror("FORK FAIL");
        exit(-1);
      }
    }
    char id[MAX_USER_ID];
    memset(buf, '\0', sizeof(buf));
    if(read(0, buf, MAX_MSG) > 0) {
      switch(get_command_type(strtok(buf, "\n"))) {
        case LIST: // LIST
          list_users(-1, user_list);
          break;
        case KICK: // KICK
          memset(id, '\0', sizeof(id));
          if (extract_name(buf, id)) {
            break;
          }
          if (find_user_index(user_list, id) == -1) {
            printf("Cannot find user: %s\n", id);
            // if trying to kick non-registered user, show this message
            break;
          }
          kick_user(find_user_index(user_list, id), user_list);
          break;
        case EXIT: // EXIT
          for (int i = 0; i < MAX_USER; i++) {
            if (user_list[i].m_status == SLOT_FULL) {
              kick_user(i, user_list);
            }
          }
          exit(0);
        default:
          broadcast_msg(user_list, buf, "admin");
          break;
      }
      print_prompt("admin");
      memset(buf, '\0', sizeof(buf));
    }

    char nBuf[MAX_MSG];
    char nText[MAX_MSG];
    char nID[MAX_MSG];
    memset(nBuf, '\0', sizeof(nBuf));
    memset(nText, '\0', sizeof(nText));
    memset(nID, '\0', sizeof(nID));
    memset(buf, '\0', sizeof(buf));
    for (int i = 0; i < MAX_USER; i++) {
      int u = read(user_list[i].m_fd_to_server, buf, MAX_MSG);
      if (u == 0) {
        kick_user(i, user_list);
        printf("\nThe user: %s seems to be terminated\n", user_list[i].m_user_id);
        print_prompt("admin");
        memset(buf, '\0', sizeof(buf));
      } else if (u > 0) {
        switch (get_command_type(strtok(buf, "\n"))) {
          case LIST: // list
            printf("LIST\n");
            // print out the LIST on server when user use the list command
            list_users(i, user_list);
            print_prompt("admin");
            break;
          case P2P: // p2p
            strcpy(nBuf, buf);
            if(extract_name(buf, nID) == -1) {
              strcpy(nText, "User was not given");
              // The case when user types "\p2p" but not user names
              write(user_list[i].m_fd_to_user, nText, sizeof(nText));
              break;
            } else if (find_user_index(user_list, nID) == -1) {
              strcpy(nText, "User not found");
              // The case when use types "\p2p username" but when username is not on the server
              write(user_list[i].m_fd_to_user, nText, sizeof(nText));
              break;
            } else {
              if(extract_text(nBuf, nText) == -1) {
                strcpy(nText, "Message is not given");
                // The case when message was not given
                write(user_list[i].m_fd_to_user, nText, sizeof(nText));
                break;
              }
              printf("msg : %s\n", nText);
              // showing the message on server that message was sent by using p2p
              printf("msg : %s : %s\n", user_list[i].m_user_id, nText);
              // showing on the server that who used p2p and what was written
              print_prompt("admin");
              send_p2p_msg(i, user_list, buf);
              break;
            }
          case EXIT: // Exit
            printf("\nThe user: %s seems to be terminated\n", user_list[i].m_user_id);
            // when using exit, showing this message
            kick_user(i, user_list);
            print_prompt("admin");
            break;
          default:
            broadcast_msg(user_list, buf, user_list[i].m_user_id);
            // Other than list, p2p or exit, just broadcast between users
            break;
        }
      }
    }
    usleep(50);
    memset(buf, '\0', sizeof(buf));

		/* ------------------------YOUR CODE FOR MAIN--------------------------------*/
	}
}

/* --------------------End of the main function ----------------------------------------*/
