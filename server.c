#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#define PORT 10000
#define AIRPLANE "./db/airplane"
#define BOOKING "./db/booking"
#define PASS_LENGTH 30
struct account
{
	int id;
	char name[10];
	char pass[PASS_LENGTH];
};

struct airplane
{
	int tid;
	char airplane_name[20];
	int airplane_no;
	int av_seats;
	int last_seatno_used;
	char departure[50];
	char arrival[50];
	int price;
	char date[11];
	char boarding_time[7];
};

struct bookings
{
	int bid;
	int type;
	int acc_no;
	int tr_id;
	char airplanename[20];
	int seat_start;
	int seat_end;
	int cancelled;
	char departure[50];
	char arrival[50];
	int price;
	char date[11];
	char boarding_time[7];
};

// create a variable to contain data storage paths
char *ACC[3] = {"./db/accounts/customer", "./db/accounts/agent", "./db/accounts/admin"};

// Interface to manage server methods
void talk_to_client(int sock);
int login(int sock);
int signup(int sock);
int menu2(int sock, int id);
int menu1(int sock, int id, int type);
void view_booking(int sock, int id, int type);
void view_booking2(int sock, int id, int type, int fd);
void sighandler(int sig);

void sighandler(int sig)
{
	exit(0);
	return;
}

int main()
{

	signal(SIGTSTP, sighandler);
	signal(SIGINT, sighandler);
	signal(SIGQUIT, sighandler);
	int sockfd = socket(AF_INET, SOCK_STREAM, 0); // Socket creation
	if (sockfd == -1)
	{
		printf("socket creation failed\n");
		exit(0);
	}
	int optval = 1; // <=> true
	int optlen = sizeof(optval);
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1)
	{
		// helps in manipulating options for the socket referred by the file descriptor sockfd
		// helps in reuse of address and port
		// Prevents error such as: “address already in use”.
		/*
		int setsockopt(int sockfd, int level, int optname,  const void *optval, socklen_t optlen);
			- level:
			+ SOL_SOCKET: This level is used for socket options that are applicable to all socket types.
			+ SOL_TCP or IPPROTO_TCP: These levels are used for TCP-specific options.
			- optname: parameter specifies the specific socket option you want to set
			+ SO_REUSEADDR to allow reusing a local address or TCP_NODELAY to enable the TCP no-delay feature
			- opval: a pointer to the value you want to set for the specified option
			- optlen: specifies the size (in bytes) of the data pointed to by optval
		*/
		printf("set socket options failed\n");
		exit(0);
	}
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = htonl(INADDR_ANY); // when you bind a socket to INADDR_ANY, the socket can accept incoming connections
	// htonl() ensures that the value is properly represented in big-endian format.

	sa.sin_port = htons(PORT);

	if (bind(sockfd, (struct sockaddr *)&sa, sizeof(sa)) == -1)
	{
		// bind() function is used in network programming with sockets to associate a socket with a specific local address
		// (IP address and port number).
		printf("binding port failed\n");
		exit(0);
	}
	if (listen(sockfd, 100) == -1)
	{
		// put socket to passive mode, waiting for client to connect
		// 100: maximum number of connections established
		printf("listen failed\n");
		exit(0);
	}
	while (1)
	{
		int connectedfd;
		if ((connectedfd = accept(sockfd, (struct sockaddr *)NULL, NULL)) == -1)
		{
			// accept an incoming connection request on a server socket and create
			// a new socket for communication with the client.
			printf("connection error\n");
			exit(0);
		}
		pthread_t cli;
		if (fork() == 0)
			talk_to_client(connectedfd);
	}

	close(sockfd);
	return 0;
}

void talk_to_client(int sock)
{
	// The read() function shall attempt to read nbyte bytes
	// from the file associated with the open file descriptor, fildes, into the buffer pointed to by buf.
	// If no process has the pipe open for writing, read() shall return 0 to indicate end-of-file.
	int func_id;
	read(sock, &func_id, sizeof(int));
	printf("Client [%d] connected\n", sock);
	while (1)
	{
		if (func_id == 1)
		{

			login(sock);
			read(sock, &func_id, sizeof(int));
		}
		if (func_id == 2)
		{
			signup(sock);
			read(sock, &func_id, sizeof(int));
		}
		if (func_id == 3)
			break;
	}
	close(sock);
	printf("Client [%d] disconnected\n", sock);
}

int login(int sock)
{
	int type, acc_no, fd, valid = 1, invalid = 0, login_success = 0;
	char password[PASS_LENGTH];
	struct account temp;
	read(sock, &type, sizeof(type));
	read(sock, &acc_no, sizeof(acc_no));
	read(sock, &password, sizeof(password));

	if ((fd = open(ACC[type - 1], O_RDWR)) == -1)
		printf("File Error\n");
	struct flock lock;

	lock.l_start = (acc_no - 1) * sizeof(struct account);
	lock.l_len = sizeof(struct account);
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();

	if (type == 1)
	{ // User
		lock.l_type = F_WRLCK;
		fcntl(fd, F_SETLK, &lock);
		lseek(fd, (acc_no - 1) * sizeof(struct account), SEEK_CUR);
		read(fd, &temp, sizeof(struct account));
		//printf("%s%s",temp.pass,password);
		if (temp.id == acc_no)
		{
			if (!strcmp(temp.pass, password))
			{
				write(sock, &valid, sizeof(valid));
				while (-1 != menu1(sock, temp.id, type))
					;
				login_success = 1;
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
		if (login_success)
			return 3;
	}
	else if (type == 2)
	{ // Agent
		// sem_t mutex; //counting semaphore for Agent login
		// sem_init(&mutex, 0, 5);

		lock.l_type = F_RDLCK;
		// sem_wait(&mutex);
		fcntl(fd, F_SETLK, &lock);
		lseek(fd, (acc_no - 1) * sizeof(struct account), SEEK_SET);
		read(fd, &temp, sizeof(struct account));
		if (temp.id == acc_no)
		{
			if (!strcmp(temp.pass, password))
			{
				write(sock, &valid, sizeof(valid));
				while (-1 != menu1(sock, temp.id, type))
					;
				login_success = 1;
			}
		}
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
		// sem_post(&mutex);
		if (login_success)
			return 3;

		/*
				read(fd, &temp, sizeof(struct account));
				if(temp.id == acc_no){
					if(!strcmp(temp.pass, password)){
						write(sock, &valid, sizeof(valid));
						close(fd);
						return 3;
					}
				}
				close(fd);
		*/
	}
	else if (type == 3)
	{
		// Admin
		lock.l_type = F_WRLCK;
		int ret = fcntl(fd, F_SETLK, &lock);
		if (ret != -1)
		{

			lseek(fd, (acc_no - 1) * sizeof(struct account), SEEK_SET); ///////CUR
			read(fd, &temp, sizeof(struct account));
			if (temp.id == acc_no)
			{
				if (!strcmp(temp.pass, password))
				{
					write(sock, &valid, sizeof(valid));
					while (-1 != menu2(sock, temp.id))
						;
					login_success = 1;
				}
			}
			lock.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &lock);
		}
		close(fd);
		if (login_success)
			return 3;
	}
	write(sock, &invalid, sizeof(invalid));
	return 3;
}

int signup(int sock)
{
	int type, fd, acc_no = 0;
	char password[PASS_LENGTH], name[10];
	struct account temp;

	read(sock, &type, sizeof(type));
	read(sock, &name, sizeof(name));
	read(sock, &password, sizeof(password));

	if ((fd = open(ACC[type - 1], O_RDWR)) == -1)
		printf("File Error\n");
	struct flock lock;
	lock.l_type = F_WRLCK;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();

	fcntl(fd, F_SETLKW, &lock);

	int fp = lseek(fd, 0, SEEK_END);

	if (fp == 0)
	{ // 1st signup
		temp.id = 1;
		strcpy(temp.name, name);
		strcpy(temp.pass, password);
		write(fd, &temp, sizeof(temp));
		write(sock, &temp.id, sizeof(temp.id));
	}
	else
	{
		fp = lseek(fd, -1 * sizeof(struct account), SEEK_END);
		read(fd, &temp, sizeof(temp));
		temp.id++;
		strcpy(temp.name, name);
		strcpy(temp.pass, password);
		write(fd, &temp, sizeof(temp));
		write(sock, &temp.id, sizeof(temp.id));
	}

	lock.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &lock);

	close(fd);
	return 3;
}


int menu2(int sock, int id)
{
	int op_id;
	read(sock, &op_id, sizeof(op_id));
	printf("Debug is going here %d\n", op_id);
	if (op_id == 1)
	{
		// add a airplane
		int tid = 0;
		int tno;
		char tname[20];
		char departure[50];
		char arrival[50];
		int price;
		char date[11];
		char boarding_time[7];
		read(sock, &tname, sizeof(tname));
		tname[sizeof(tname) - 1] = '\0';
		printf("airplane name: %s\n", tname);
		read(sock, &tno, sizeof(tno));
		read(sock, &departure, sizeof(departure));
		read(sock, &arrival, sizeof(arrival));
		read(sock, &price, sizeof(price));
		read(sock, &date, sizeof(date));
		read(sock, &boarding_time, sizeof(boarding_time));

		struct airplane temp, temp2;

		temp.tid = tid;
		temp.airplane_no = tno;
		strcpy(temp.airplane_name, tname);
		strcpy(temp.departure, departure);
		strcpy(temp.arrival, arrival);
		temp.price = price;
		strcpy(temp.date, date);
		strcpy(temp.boarding_time, boarding_time);
		temp.av_seats = 185;
		temp.last_seatno_used = 0;

		int fd = open(AIRPLANE, O_RDWR);
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		if (fp == 0)
		{
			write(fd, &temp, sizeof(temp));
			lock.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &lock);
			close(fd);
			write(sock, &op_id, sizeof(op_id));
		}
		else
		{
			lseek(fd, -1 * sizeof(struct airplane), SEEK_CUR);
			read(fd, &temp2, sizeof(temp2));
			temp.tid = temp2.tid + 1;
			write(fd, &temp, sizeof(temp));
			write(sock, &op_id, sizeof(op_id));
			lock.l_type = F_UNLCK;
			fcntl(fd, F_SETLK, &lock);
			close(fd);
		}
		return op_id;
	}
	if (op_id == 2)
	{
		int fd = open(AIRPLANE, O_RDWR);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		int no_of_airplanes = fp / sizeof(struct airplane);
		printf("no of airplane:%d\n", no_of_airplanes);
		write(sock, &no_of_airplanes, sizeof(int));
		lseek(fd, 0, SEEK_SET);
		struct airplane temp;
		while (fp != lseek(fd, 0, SEEK_CUR))
		{
			printf("FP :%d  FD :%ld\n", fp, lseek(fd, 0, SEEK_CUR));
			read(fd, &temp, sizeof(struct airplane));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.airplane_name, sizeof(temp.airplane_name));
			write(sock, &temp.airplane_no, sizeof(int));
			write(sock, &temp.departure, sizeof(temp.departure));
			write(sock, &temp.arrival, sizeof(temp.arrival));
			write(sock, &temp.price, sizeof(int));
			write(sock, &temp.date, sizeof(temp.date));
			write(sock, &temp.boarding_time, sizeof(temp.boarding_time));
		}
		// int airplane_id=-1;
		read(sock, &no_of_airplanes, sizeof(int));
		if (no_of_airplanes != -2) // write(sock, &no_of_airplanes, sizeof(int));
		{
			struct airplane temp;
			// lseek(fd, 0, SEEK_SET);
			lseek(fd, (no_of_airplanes) * sizeof(struct airplane), SEEK_SET);
			read(fd, &temp, sizeof(struct airplane));
			printf("%s is deleted\n", temp.airplane_name);
			strcpy(temp.airplane_name, "deleted");
			lseek(fd, -1 * sizeof(struct airplane), SEEK_CUR);
			write(fd, &temp, sizeof(struct airplane));
			// write(sock, &no_of_airplanes, sizeof(int));
		}
		write(sock, &no_of_airplanes, sizeof(int));
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
	}
	if (op_id == 3)
	{
		int fd = open(AIRPLANE, O_RDWR);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		int no_of_airplanes = fp / sizeof(struct airplane);
		write(sock, &no_of_airplanes, sizeof(int));
		lseek(fd, 0, SEEK_SET);
		while (fp != lseek(fd, 0, SEEK_CUR))
		{
			struct airplane temp;
			read(fd, &temp, sizeof(struct airplane));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.airplane_name, sizeof(temp.airplane_name));
			write(sock, &temp.airplane_no, sizeof(int));
			write(sock, &temp.departure, sizeof(temp.departure));
			write(sock, &temp.arrival, sizeof(temp.arrival));
			write(sock, &temp.price, sizeof(int));
			write(sock, &temp.date, sizeof(temp.date));
			write(sock, &temp.boarding_time, sizeof(temp.boarding_time));
		}
		read(sock, &no_of_airplanes, sizeof(int));

		struct airplane temp;
		lseek(fd, 0, SEEK_SET);
		lseek(fd, (no_of_airplanes - 1) * sizeof(struct airplane), SEEK_CUR);
		read(fd, &temp, sizeof(struct airplane));

		read(sock, &no_of_airplanes, sizeof(int));
		printf("Debug seat is here: %d\n", no_of_airplanes);
		printf("Welcome to 1337\n");
		if (no_of_airplanes == 1)
		{
			char name[20];
			write(sock, &temp.airplane_name, sizeof(temp.airplane_name));
			read(sock, &name, sizeof(name));
			strcpy(temp.airplane_name, name);
		}
		else if (no_of_airplanes == 2)
		{
			write(sock, &temp.airplane_no, sizeof(temp.airplane_no));
			read(sock, &temp.airplane_no, sizeof(temp.airplane_no));
		}
		else if (no_of_airplanes == 3)
		{
			int test = temp.av_seats;
			write(sock, &test, sizeof(test));
			printf("Number of seat before change is: %d\n", test);
			read(sock, &temp.av_seats, sizeof(temp.av_seats));
		}
		// else if (no_of_airplanes == 4){
		// update new fields to modify
		// }
		no_of_airplanes = 3;
		printf("%s\t%d\t%d\t%s\t%s\t%d\t%s\t%s\n", temp.airplane_name, temp.airplane_no, temp.av_seats, temp.departure, temp.arrival, temp.price, temp.date, temp.boarding_time);
		lseek(fd, -1 * sizeof(struct airplane), SEEK_CUR);
		write(fd, &temp, sizeof(struct airplane));
		write(sock, &no_of_airplanes, sizeof(int));

		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);
		close(fd);
		return op_id;
	}
	if (op_id == 4)
	{
		int type = 3, fd, acc_no = 0;
		char password[PASS_LENGTH], name[10];
		struct account temp;
		read(sock, &name, sizeof(name));
		read(sock, &password, sizeof(password));

		if ((fd = open(ACC[type - 1], O_RDWR)) == -1)
			printf("File Error\n");
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);
		int fp = lseek(fd, 0, SEEK_END);
		fp = lseek(fd, -1 * sizeof(struct account), SEEK_CUR);
		read(fd, &temp, sizeof(temp));
		temp.id++;
		strcpy(temp.name, name);
		strcpy(temp.pass, password);
		write(fd, &temp, sizeof(temp));
		write(sock, &temp.id, sizeof(temp.id));
		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);

		close(fd);
		op_id = 4;
		write(sock, &op_id, sizeof(op_id));
		return op_id;
	}
	if (op_id == 5)
	{
		int type, id;
		struct account var;
		read(sock, &type, sizeof(type));

		int fd = open(ACC[type - 1], O_RDWR);
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_whence = SEEK_SET;
		lock.l_len = 0;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		int fp = lseek(fd, 0, SEEK_END);
		int users = fp / sizeof(struct account);
		write(sock, &users, sizeof(int));

		lseek(fd, 0, SEEK_SET);
		while (fp != lseek(fd, 0, SEEK_CUR))
		{
			read(fd, &var, sizeof(struct account));
			write(sock, &var.id, sizeof(var.id));
			write(sock, &var.name, sizeof(var.name));
		}

		read(sock, &id, sizeof(id));
		if (id == 0)
		{
			write(sock, &op_id, sizeof(op_id));
		}
		else
		{
			lseek(fd, 0, SEEK_SET);
			lseek(fd, (id - 1) * sizeof(struct account), SEEK_CUR);
			read(fd, &var, sizeof(struct account));
			lseek(fd, -1 * sizeof(struct account), SEEK_CUR);
			strcpy(var.name, "deleted");
			strcpy(var.pass, "");
			write(fd, &var, sizeof(struct account));
			write(sock, &op_id, sizeof(op_id));
		}

		lock.l_type = F_UNLCK;
		fcntl(fd, F_SETLK, &lock);

		close(fd);

		return op_id;
	}
	if (op_id == 6)
	{
		write(sock, &op_id, sizeof(op_id));
		return -1;
	}
}

int menu1(int sock, int id, int type)
{
	int op_id;
	read(sock, &op_id, sizeof(op_id));
	if (op_id == 1)
	{
		// book a ticket
		int fd = open(AIRPLANE, O_RDWR);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		struct airplane temp;
		int fp = lseek(fd, 0, SEEK_END);
		int no_of_airplanes = fp / sizeof(struct airplane);
		write(sock, &no_of_airplanes, sizeof(int));
		lseek(fd, 0, SEEK_SET);
		while (fp != lseek(fd, 0, SEEK_CUR))
		{
			read(fd, &temp, sizeof(struct airplane));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.airplane_no, sizeof(int));
			write(sock, &temp.av_seats, sizeof(int));
			write(sock, &temp.airplane_name, sizeof(temp.airplane_name));
			write(sock, &temp.departure, sizeof(temp.departure));
			write(sock, &temp.arrival, sizeof(temp.arrival));
			write(sock, &temp.price, sizeof(int));
			write(sock, &temp.date, sizeof(temp.date));
			write(sock, &temp.boarding_time, sizeof(temp.boarding_time));
		}
		// struct airplane temp1;
		memset(&temp, 0, sizeof(struct airplane));
		char mail[50];
		read(sock, mail, sizeof(mail));
		int airplaneid, seats;
		read(sock, &airplaneid, sizeof(airplaneid));
		// lseek(fd, 0, SEEK_SET);
		lseek(fd, airplaneid * sizeof(struct airplane), SEEK_SET);
		read(fd, &temp, sizeof(struct airplane));
		write(sock, &temp.av_seats, sizeof(int));
		read(sock, &seats, sizeof(seats));
		if (seats > 0)
		{
			temp.av_seats -= seats;
			int fd2 = open(BOOKING, O_RDWR);
			fcntl(fd2, F_SETLKW, &lock);
			struct bookings bk;
			int fp2 = lseek(fd2, 0, SEEK_END);
			if (fp2 > 0)
			{
				lseek(fd2, -1 * sizeof(struct bookings), SEEK_CUR);
				read(fd2, &bk, sizeof(struct bookings));
				bk.bid++;
			}
			else
				bk.bid = 0;
			bk.type = type;
			bk.acc_no = id;
			bk.tr_id = airplaneid;
			bk.cancelled = 0;
			strcpy(bk.airplanename, temp.airplane_name);
			strcpy(bk.departure, temp.departure);
			strcpy(bk.arrival, temp.arrival);
			bk.price = temp.price;
			strcpy(bk.date, temp.date);
			strcpy(bk.boarding_time, temp.boarding_time);
			printf("airplanename: %s\n", bk.airplanename);
			printf("departure: %s\n", bk.departure);
			printf("arrival: %s\n", bk.arrival);
			printf("price: %d$\n", bk.price * seats);
			printf("date: %s\n", bk.date);
			printf("boarding_time: %s\n", bk.boarding_time);
			char command[1000], command2[1000];
			sprintf(command2,
    "echo \"Subject: Flight Booking Confirmation\nContent-Type: text/html\n\n<html><body style='font-family: Arial, sans-serif; padding: 20px;'><h2 style='color: #3498db;'>Flight Booking Confirmation</h2><p>Dear our dearest customer,</p><p>We are delighted to inform you that your flight booking has been confirmed. Please check on the system for the details of your booking:</p><ul style='list-style-type: none; padding: 0;'><li><strong>Email:</strong> %s</li><li><strong>Carrier:</strong> %s</li><li><strong>Departure:</strong> %s</li><li><strong>Arrival:</strong> %s</li><li><strong>Date:</strong> %s</li><li><strong>Boarding time:</strong> %s</li></ul><p>Thank you for choosing our services. Have a pleasant journey!</p><p style='color: #555;'>Best regards,<br>Your Airline Team</p></body></html>\" > email2.html",
    mail, bk.airplanename, bk.departure, bk.arrival, bk.date, bk.boarding_time);
			system(command2);
			sprintf(command,
					"curl -s --url 'smtp://smtp.gmail.com:587' --ssl-reqd "
					"--mail-from 'toniminh161200@gmail.com' --mail-rcpt %s "
					"--user 'toniminh161200@gmail.com:tkxj thcp fpzf exna' --tlsv1.2 --upload-file email2.html",
					mail);
			printf("Email has been sent sucesfully to: %s",mail);
			system(command);
			bk.seat_start = temp.last_seatno_used + 1;
			bk.seat_end = temp.last_seatno_used + seats;
			temp.last_seatno_used = bk.seat_end;
			write(fd2, &bk, sizeof(bk));
			lock.l_type = F_UNLCK;
			fcntl(fd2, F_SETLK, &lock);
			close(fd2);
			lseek(fd, -1 * sizeof(struct airplane), SEEK_CUR);
			write(fd, &temp, sizeof(temp));
		}
		fcntl(fd, F_SETLK, &lock);
		close(fd);

		if (seats <= 0)
			op_id = -1;
		write(sock, &op_id, sizeof(op_id));
		return 1;
	}
	if (op_id == 2)
	{
		view_booking(sock, id, type);
		write(sock, &op_id, sizeof(op_id));
		return 2;
	}
	if (op_id == 3)
	{
		// update booking
		view_booking(sock, id, type);

		int fd1 = open(AIRPLANE, O_RDWR);
		int fd2 = open(BOOKING, O_RDWR);
		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd1, F_SETLKW, &lock);
		fcntl(fd2, F_SETLKW, &lock);

		int val;
		struct airplane temp1;
		struct bookings temp2;
		read(sock, &val, sizeof(int));
		lseek(fd2, 0, SEEK_SET);
		lseek(fd2, val * sizeof(struct bookings), SEEK_CUR);
		read(fd2, &temp2, sizeof(temp2));
		lseek(fd2, -1 * sizeof(struct bookings), SEEK_CUR);
		printf("%d %s %d\n", temp2.tr_id, temp2.airplanename, temp2.seat_end);

		lseek(fd1, 0, SEEK_SET);
		lseek(fd1, (temp2.tr_id - 1) * sizeof(struct airplane), SEEK_CUR);
		read(fd1, &temp1, sizeof(temp1));
		lseek(fd1, -1 * sizeof(struct airplane), SEEK_CUR);
		printf("%d %s %d\n", temp1.tid, temp1.airplane_name, temp1.av_seats);

		read(sock, &val, sizeof(int));

		if (val == 1)
		{
			read(sock, &val, sizeof(int));
			if (temp1.av_seats >= val)
			{
				temp2.cancelled = 1;
				temp1.av_seats += val;
				write(fd2, &temp2, sizeof(temp2));

				int tot_seats = temp2.seat_end - temp2.seat_start + 1 + val;
				struct bookings bk;

				int fp2 = lseek(fd2, 0, SEEK_END);
				lseek(fd2, -1 * sizeof(struct bookings), SEEK_CUR);
				read(fd2, &bk, sizeof(struct bookings));

				bk.bid++;
				bk.type = temp2.type;
				bk.acc_no = temp2.acc_no;
				bk.tr_id = temp2.tr_id;
				bk.cancelled = 0;
				strcpy(bk.airplanename, temp2.airplanename);
				bk.seat_start = temp1.last_seatno_used + 1;
				bk.seat_end = temp1.last_seatno_used + tot_seats;

				temp1.av_seats -= tot_seats;
				temp1.last_seatno_used = bk.seat_end;

				write(fd2, &bk, sizeof(bk));
				write(fd1, &temp1, sizeof(temp1));
			}
			else
			{
				op_id = -2;
				write(sock, &op_id, sizeof(op_id));
			}
		}
		else
		{								   // decrease
			read(sock, &val, sizeof(int)); // No of Seats
			if (temp2.seat_end - val < temp2.seat_start)
			{
				temp2.cancelled = 1;
				temp1.av_seats += val;
			}
			else
			{
				temp2.seat_end -= val;
				temp1.av_seats += val;
			}
			write(fd2, &temp2, sizeof(temp2));
			write(fd1, &temp1, sizeof(temp1));
		}
		lock.l_type = F_UNLCK;
		fcntl(fd1, F_SETLK, &lock);
		fcntl(fd2, F_SETLK, &lock);
		close(fd1);
		close(fd2);
		if (op_id > 0)
			write(sock, &op_id, sizeof(op_id));
		return 3;
	}
	if (op_id == 4)
	{
		// cancel booking
		view_booking(sock, id, type);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		int fd1 = open(AIRPLANE, O_RDWR);
		int fd2 = open(BOOKING, O_RDWR);
		fcntl(fd1, F_SETLKW, &lock);

		int val;
		struct airplane temp1;
		struct bookings temp2;
		read(sock, &val, sizeof(int));
		lseek(fd2, val * sizeof(struct bookings), SEEK_SET);

		lock.l_start = val * sizeof(struct bookings);
		lock.l_len = sizeof(struct bookings);
		fcntl(fd2, F_SETLKW, &lock);

		read(fd2, &temp2, sizeof(temp2));
		lseek(fd2, -1 * sizeof(struct bookings), SEEK_CUR);
		printf("%d %s %d\n", temp2.tr_id, temp2.airplanename, temp2.seat_end);

		lseek(fd1, (temp2.tr_id) * sizeof(struct airplane), SEEK_SET);
		lock.l_start = (temp2.tr_id) * sizeof(struct airplane);
		lock.l_len = sizeof(struct airplane);
		fcntl(fd1, F_SETLKW, &lock);
		read(fd1, &temp1, sizeof(temp1));
		lseek(fd1, -1 * sizeof(struct airplane), SEEK_CUR);
		printf("%d %s %d\n", temp1.tid, temp1.airplane_name, temp1.av_seats);
		temp1.av_seats += temp2.seat_end - temp2.seat_start + 1;
		temp2.cancelled = 1;
		write(fd2, &temp2, sizeof(temp2));
		write(fd1, &temp1, sizeof(temp1));

		lock.l_type = F_UNLCK;
		fcntl(fd1, F_SETLK, &lock);
		fcntl(fd2, F_SETLK, &lock);
		close(fd1);
		close(fd2);
		write(sock, &op_id, sizeof(op_id));
		return 4;
	}
	if (op_id == 5)
	{
		// searching information
		int fd = open(AIRPLANE, O_RDWR);

		struct flock lock;
		lock.l_type = F_WRLCK;
		lock.l_start = 0;
		lock.l_len = 0;
		lock.l_whence = SEEK_SET;
		lock.l_pid = getpid();

		fcntl(fd, F_SETLKW, &lock);

		struct airplane temp;
		int fp = lseek(fd, 0, SEEK_END);
		int no_of_airplanes = fp / sizeof(struct airplane);
		write(sock, &no_of_airplanes, sizeof(int));
		lseek(fd, 0, SEEK_SET);
		while (fp != lseek(fd, 0, SEEK_CUR))
		{
			read(fd, &temp, sizeof(struct airplane));
			write(sock, &temp.tid, sizeof(int));
			write(sock, &temp.airplane_no, sizeof(int));
			write(sock, &temp.av_seats, sizeof(int));
			write(sock, &temp.airplane_name, sizeof(temp.airplane_name));
			write(sock, &temp.departure, sizeof(temp.departure));
			write(sock, &temp.arrival, sizeof(temp.arrival));
			write(sock, &temp.price, sizeof(int));
			write(sock, &temp.date, sizeof(temp.date));
			write(sock, &temp.boarding_time, sizeof(temp.boarding_time));
		}
		// struct airplane temp1;
		memset(&temp, 0, sizeof(struct airplane));
		lseek(fd, 0, SEEK_SET);
		int found_airplanes = 0;
		int search_option = -1;
		char search_query[50];
		read(sock, &search_option, sizeof(search_option));
		read(sock, &search_query, sizeof(search_query));
		printf("search_option: %d\n", search_option);
		printf("search_query: %s\n", search_query);

		struct airplane matching_airplanes[no_of_airplanes];

		for (int i = 1; i <= no_of_airplanes; i++)
		{
			struct airplane temp1;
			read(fd, &temp1, sizeof(struct airplane));
			
			// Check for a match based on the search option
			int match = 0;
			switch (search_option)
			{
			case 1: // Search by airplane name
				if (strstr(temp1.airplane_name, search_query) != NULL)
					match = 1;
				matching_airplanes[found_airplanes] = temp1;
				break;
			case 2: // Search by departure
				if (strstr(temp1.departure, search_query) != NULL)
					match = 1;
				matching_airplanes[found_airplanes] = temp1;
				break;
			case 3: // Search by arrival
				if (strstr(temp1.arrival, search_query) != NULL)
					match = 1;
				matching_airplanes[found_airplanes] = temp1;
				break;
			case 4: // Search by date
				if (strstr(temp1.date, search_query) != NULL)
					match = 1;
				matching_airplanes[found_airplanes] = temp1;
				break;
			}

			if (match == 1)
			{
				found_airplanes++;
			}
		}
		write(sock, &found_airplanes, sizeof(found_airplanes));
		for (int i = 0; i < found_airplanes; i++)
		{
			write(sock, &matching_airplanes[i].tid, sizeof(int));
			write(sock, &matching_airplanes[i].airplane_no, sizeof(int));
			write(sock, &matching_airplanes[i].av_seats, sizeof(int));
			write(sock, &matching_airplanes[i].airplane_name, sizeof(matching_airplanes[i].airplane_name));
			write(sock, &matching_airplanes[i].departure, sizeof(matching_airplanes[i].departure));
			write(sock, &matching_airplanes[i].arrival, sizeof(matching_airplanes[i].arrival));
			write(sock, &matching_airplanes[i].price, sizeof(int));
			write(sock, &matching_airplanes[i].date, sizeof(matching_airplanes[i].date));
			write(sock, &matching_airplanes[i].boarding_time, sizeof(matching_airplanes[i].boarding_time));
		}
		found_airplanes = 0;
		memset(&matching_airplanes, 0, sizeof(struct airplane) * found_airplanes);
		// Release the lock
		fcntl(fd, F_SETLK, &lock);
		close(fd);

		return 5;
	}
	if (op_id == 6)
	{
		write(sock, &op_id, sizeof(op_id));
		return -1;
	}
}

void view_booking(int sock, int id, int type)
{
	int fd = open(BOOKING, O_RDONLY);
	struct flock lock;
	lock.l_type = F_RDLCK;
	lock.l_start = 0;
	lock.l_len = 0;
	lock.l_whence = SEEK_SET;
	lock.l_pid = getpid();

	fcntl(fd, F_SETLKW, &lock);

	int fp = lseek(fd, 0, SEEK_END);
	int entries = 0;
	if (fp == 0)
		write(sock, &entries, sizeof(entries));
	else
	{
		struct bookings bk[10];
		while (fp > 0 && entries < 10)
		{
			struct bookings temp;
			fp = lseek(fd, -1 * sizeof(struct bookings), SEEK_CUR);
			read(fd, &temp, sizeof(struct bookings));
			if (temp.acc_no == id && temp.type == type)
				bk[entries++] = temp;
			fp = lseek(fd, -1 * sizeof(struct bookings), SEEK_CUR);
		}
		write(sock, &entries, sizeof(entries));
		for (fp = 0; fp < entries; fp++)
		{
			write(sock, &bk[fp].bid, sizeof(bk[fp].bid));
			write(sock, &bk[fp].airplanename, sizeof(bk[fp].airplanename));
			write(sock, &bk[fp].seat_start, sizeof(int));
			write(sock, &bk[fp].seat_end, sizeof(int));
			write(sock, &bk[fp].cancelled, sizeof(int));
			write(sock, &bk[fp].departure, sizeof(bk[fp].departure));
			write(sock, &bk[fp].arrival, sizeof(bk[fp].arrival));
			write(sock, &bk[fp].price, sizeof(int));
			write(sock, &bk[fp].date, sizeof(bk[fp].date));
			write(sock, &bk[fp].boarding_time, sizeof(bk[fp].boarding_time));
		}
	}
	lock.l_type = F_UNLCK;
	fcntl(fd, F_SETLK, &lock);
	close(fd);
}
