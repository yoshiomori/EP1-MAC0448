/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 12/08/2013
 * 
 * Um código simples (não é o código ideal, mas é o suficiente para o
 * EP) de um servidor de eco a ser usado como base para o EP1. Ele
 * recebe uma linha de um cliente e devolve a mesma linha. Teste ele
 * assim depois de compilar:
 * 
 * ./servidor 8000
 * 
 * Com este comando o servidor ficará escutando por conexões na porta
 * 8000 TCP (Se você quiser fazer o servidor escutar em uma porta
 * menor que 1024 você precisa ser root).
 *
 * Depois conecte no servidor via telnet. Rode em outro terminal:
 * 
 * telnet 127.0.0.1 8000
 * 
 * Escreva sequências de caracteres seguidas de ENTER. Você verá que
 * o telnet exibe a mesma linha em seguida. Esta repetição da linha é
 * enviada pelo servidor. O servidor também exibe no terminal onde ele
 * estiver rodando as linhas enviadas pelos clientes.
 * 
 * Obs.: Você pode conectar no servidor remotamente também. Basta saber o
 * endereço IP remoto da máquina onde o servidor está rodando e não
 * pode haver nenhum firewall no meio do caminho bloqueando conexões na
 * porta escolhida.
 */
	
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096
	
void strtolower (char *str)
{
  for (;*str; ++str)
    *str = tolower (*str);
}
	
int main (int argc, char **argv)
{
  char *cmd, arg1[MAXLINE + 1], addr[MAXLINE + 1], *c, p1[MAXLINE + 1], p2[MAXLINE + 1], *cmd_file_arg, cmd_file[MAXLINE + 1], dataline[MAXLINE + 1];
  int port = 1024, fd[2], data_reply_fd[2];
  FILE * pFile, *fpin, *fpout;
  /* Os sockets. Um que será o socket que vai escutar pelas conexões
   * e o outro que vai ser o socket específico de cada conexão */
  int listenfd, listendatafd, connfd, conndatafd;
  socklen_t len;
  /* Informações sobre o socket (endereço e porta) ficam nesta struct */
  struct sockaddr_in server_PI_socket, server_DTP_socket, clientaddr;
  /* Retorno da função fork para saber quem é o processo filho e quem
   * é o processo pai */
  pid_t childpid, grandchildid;
  /* Armazena linhas recebidas do cliente */
  char	recvline[MAXLINE + 1], respline[MAXLINE + 1], cmdfile[MAXLINE + 1];
  /* Armazena o tamanho da string lida do cliente */
  ssize_t  n;

  if (argc != 2)
    {
      fprintf (stderr,"Uso: %s <Porta>\n",argv[0]);
      fprintf (stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
      exit (1);
    }
	
  /* Criação de um socket. Eh como se fosse um descritor de arquivo. Eh
   * possivel fazer operacoes como read, write e close. Neste
   * caso o socket criado eh um socket IPv4 (por causa do AF_INET),
   * que vai usar TCP (por causa do SOCK_STREAM), já que o FTP
   * funciona sobre TCP, e será usado para uma aplicação convencional sobre
   * a Internet (por causa do número 0) */
  if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket :(\n");
      exit (2);
    }
	
  /* Agora é necessário informar os endereços associados a este
   * socket. É necessário informar o endereço / interface e a porta,
   * pois mais adiante o socket ficará esperando conexões nesta porta
   * e neste(s) endereços. Para isso é necessário preencher a struct
   * server_PI_socket. É necessário colocar lá o tipo de socket (No nosso
   * caso AF_INET porque é IPv4), em qual endereço / interface serão
   * esperadas conexões (Neste caso em qualquer uma -- INADDR_ANY) e
   * qual a porta. Neste caso será a porta que foi passada como
   * argumento no shell (atoi(argv[1]))
   */
  bzero (&server_PI_socket, sizeof (server_PI_socket));
  server_PI_socket.sin_family      = AF_INET;
  server_PI_socket.sin_addr.s_addr = htonl (INADDR_ANY);
  server_PI_socket.sin_port        = htons (atoi (argv[1]));
  if (bind (listenfd,
	    (struct sockaddr *) & server_PI_socket,
	    sizeof (server_PI_socket)) == -1) {
    perror ("bind :(\n");
    exit (3);
  }
	
  /* Como este código é o código de um servidor, o socket será um
   * socket passivo. Para isto é necessário chamar a função listen
   * que define que este é um socket de servidor que ficará esperando
   * por conexões nos endereços definidos na função bind. */
  if (listen (listenfd, LISTENQ) == -1)
    {
      perror ("listen :(\n");
      exit (4);
    }
	
  printf ("[Servidor no ar. Aguardando conexoes na porta %s]\n",argv[1]);
  printf ("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
	   
  /* O servidor no final das contas é um loop infinito de espera por
   * conexões e processamento de cada uma individualmente */
  for (;;) {
    len = sizeof (clientaddr);
    /* O socket inicial que foi criado é o socket que vai aguardar
     * pela conexão na porta especificada. Mas pode ser que existam
     * diversos clientes conectando no servidor. Por isso deve-se
     * utilizar a função accept. Esta função vai retirar uma conexão
     * da fila de conexões que foram aceitas no socket listenfd e
     * vai criar um socket específico para esta conexão. O descritor
     * deste novo socket é o retorno da função accept. */
    if ((connfd = accept (listenfd, (struct sockaddr *) &clientaddr, &len)) == -1 ) {
      perror ("accept :(\n");
      exit (5);
    }
	
    /* Agora o servidor precisa tratar este cliente de forma
     * separada. Para isto é criado um processo filho usando a
     * função fork. O processo vai ser uma cópia deste. Depois da
     * função fork, os dois processos (pai e filho) estarão no mesmo
     * ponto do código, mas cada um terá um PID diferente. Assim é
     * possível diferenciar o que cada processo terá que fazer. O
     * filho tem que processar a requisição do cliente. O pai tem
     * que voltar no loop para continuar aceitando novas conexões */
    /* Se o retorno da função fork for zero, é porque está no
     * processo filho. */
    if ( (childpid = fork ()) == 0) {
      /**** PROCESSO FILHO ****/
      printf ("[Uma conexao aberta]\n");
      /* Já que está no processo filho, não precisa mais do socket
       * listenfd. Só o processo pai precisa deste socket. */
      close (listenfd);
	         
      /* Agora pode ler do socket e escrever no socket. Isto tem
       * que ser feito em sincronia com o cliente. Não faz sentido
       * ler sem ter o que ler. Ou seja, neste caso está sendo
       * considerado que o cliente vai enviar algo para o servidor.
       * O servidor vai processar o que tiver sido enviado e vai
       * enviar uma resposta para o cliente (Que precisará estar
       * esperando por esta resposta) 
       */
	
      /* ========================================================= */
      /* ========================================================= */
      /*                         EP1 INÍCIO                        */
      /* ========================================================= */
      /* ========================================================= */
      /* TODO: É esta parte do código que terá que ser modificada
       * para que este servidor consiga interpretar comandos FTP */
      /* if(!getsockopt(connfd, 6, IP_PKTINFO, &ip, NULL)) */
      /* 	printf("%s\n",inet_ntoa(ip.ipi_addr)); */
      write (connfd, "220 ProFTPD 1.3.5rc3 Server (FTP Server Test) [127.0.0.1]\r\n", 59);
      while ((n=read (connfd, recvline, MAXLINE)) > 0){
	recvline[n]=0;
	printf ("[Cliente conectado no processo filho %d enviou:] ",getpid ()); 
	if ((fputs (recvline,stdout)) == EOF) {
	  perror ("fputs :( \n");
	  exit (6);
	}
	/* write(connfd, recvline, strlen(recvline)); */
	/* printf("%d\n", (int)strlen("331 Password required for ftp\n")); */
	cmd = strtok (recvline," \r\n");
	while (cmd != NULL){
	  strtolower (cmd);
	  if (!strcmp ("user", cmd)){
	    strcpy (arg1,strtok (NULL, " \r\n"));
	    snprintf (respline,
		      MAXLINE + 1,
		      "331 Password required for %s\r\n",
		      arg1);
	    write (connfd, respline, strlen (respline));
	  }
	  else if(!strcmp("pass", cmd)){
	    snprintf(respline, MAXLINE + 1, "230 User %s logged in\r\n", arg1);
	    write(connfd, respline, strlen(respline));
	  }
	  else if(!strcmp("pasv", cmd)){
	    pipe (fd);
	    pipe (data_reply_fd);
	    if((grandchildid = fork()) == 0){
	      /* Child process close up output side of pipe */
	      close(fd[1]);
	      close(data_reply_fd[0]);

	      /* Socket para transferência de dados */
	      if ((listendatafd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket :(\n");
		exit(2);
	      }
	      bzero(&server_DTP_socket, sizeof(server_DTP_socket));
	      server_DTP_socket.sin_family      = AF_INET;
	      server_DTP_socket.sin_addr.s_addr = server_PI_socket.sin_addr.s_addr;
	      do server_DTP_socket.sin_port     = htons(port++);
	      while(bind(listendatafd,(struct sockaddr *)&server_DTP_socket,sizeof(server_DTP_socket))==-1);
	      port--;
	      if (listen(listendatafd, LISTENQ) == -1) {
		perror("listen :(\n");
		exit(4);
	      }

	      
	      inet_ntop(AF_INET, &clientaddr.sin_addr, addr, sizeof(addr));
	      for(c = addr; *c; c++) if(*c == '.') *c = ',';
	      snprintf(respline,
		       MAXLINE + 1,
		       "227 Entering Passive Mode (%s,%d,%d)\r\n",
		       addr, port >> 8 & 0xFF, port & 0xFF);
	      write(connfd, respline, strlen(respline));

	      if((conndatafd=accept(listendatafd,(struct sockaddr *)NULL,NULL))==-1){
		perror("accept :(\n");
		exit(5);
	      }

	      /* Read in a string from the pipe */
	      for(;;){
		while((n=read(fd[0], cmdfile, MAXLINE)) > 0){
		  cmd_file_arg = strtok (cmdfile," ");
		  if(!strcmp(cmd_file_arg, "exit")){
		    shutdown(conndatafd, SHUT_RDWR);
		    close(listendatafd);
		    exit(0);
		  }
		  else if(!strcmp(cmd_file_arg, "stor")){
		    cmd_file_arg = strtok(NULL, "");
		    pFile = fopen (cmd_file_arg, "w");
		    fpin = fdopen(conndatafd, "r");
		    snprintf(respline,
			     MAXLINE + 1,
			     "150 Opening BINARY mode data connection for %s\r\n",
			     arg1);
		    write(connfd, respline, strlen(respline));
		    while (fgets(dataline, MAXLINE, fpin) != NULL)
		      fputs (dataline, pFile);
		    fclose (pFile);
		    snprintf(respline,
			     MAXLINE + 1,
			     "226 Transfer complete\r\n");
		    write(connfd, respline, strlen(respline));
		  }
		  else if(!strcmp(cmd_file_arg, "write")){
		    cmd_file_arg = strtok(NULL, "");
		    write(conndatafd, cmd_file_arg, strlen(cmd_file_arg));
		  }
		}
	      }
	    }
	    else /* else do fork */
	      {
		/* Parent process close up input side of pipe */
		close(fd[0]);
		close(data_reply_fd[0]);
	      }
	  }
	  else if(!strcmp("syst", cmd)){
	    snprintf(respline,
		     MAXLINE + 1,
		     "215 UNIX Type: L8\r\n");
	    write(connfd, respline, strlen(respline));
	  }
	  else if(!strcmp("quit", cmd)){
	    snprintf(respline,
		     MAXLINE + 1,
		     "221 Goodbye\r\n");
	    write(connfd, respline, strlen(respline));
	  }
	  else if(!strcmp("type", cmd)){
	    strcpy(arg1,strtok (NULL, " \r\n"));
	    if(!strcmp("I", arg1)){
	      snprintf(respline,
		       MAXLINE + 1,
		       "200 Type set to %s\r\n",
		       arg1);
	    }
	    else
	      snprintf(respline,
		       MAXLINE + 1,
		       "500 argument %s unrecognized\r\n",
		       arg1);
	    write(connfd, respline, strlen(respline));
	  }
	  else if (!strcmp (cmd, "stor")){
	    strcpy (arg1,strtok (NULL, "\r\n"));
	    snprintf(cmd_file, MAXLINE, "stor %s", arg1);
	    write(fd[1], cmd_file, strlen(cmd_file));
	  }
	  else if (!strcmp (cmd, "retr")){
	    strcpy (arg1, strtok (NULL, "\r\n"));
	    snprintf (cmd_file, MAXLINE, "a");
	  }
	  else
	    strcpy(respline, "500 Invalid command");
	  cmd = strtok (NULL, " \r\n");
	}
      }
      /* ========================================================= */
      /* ========================================================= */
      /*                         EP1 FIM                           */
      /* ========================================================= */
      /* ========================================================= */
	
      /* Após ter feito toda a troca de informação com o cliente,
       * pode finalizar o processo filho */
      printf("[Uma conexao fechada]\n");
      exit(0);
    }
    /**** PROCESSO PAI ****/
    /* Se for o pai, a única coisa a ser feita é fechar o socket
     * connfd (ele é o socket do cliente específico que será tratado
     * pelo processo filho) */
    close(connfd);
  }
  exit(0);
}
	
