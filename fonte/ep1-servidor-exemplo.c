/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 12/08/2013
 * 
 * Um c�digo simples (n�o � o c�digo ideal, mas � o suficiente para o
 * EP) de um servidor de eco a ser usado como base para o EP1. Ele
 * recebe uma linha de um cliente e devolve a mesma linha. Teste ele
 * assim depois de compilar:
 * 
 * ./servidor 8000
 * 
 * Com este comando o servidor ficar� escutando por conex�es na porta
 * 8000 TCP (Se voc� quiser fazer o servidor escutar em uma porta
 * menor que 1024 voc� precisa ser root).
 *
 * Depois conecte no servidor via telnet. Rode em outro terminal:
 * 
 * telnet 127.0.0.1 8000
 * 
 * Escreva sequ�ncias de caracteres seguidas de ENTER. Voc� ver� que
 * o telnet exibe a mesma linha em seguida. Esta repeti��o da linha �
 * enviada pelo servidor. O servidor tamb�m exibe no terminal onde ele
 * estiver rodando as linhas enviadas pelos clientes.
 * 
 * Obs.: Voc� pode conectar no servidor remotamente tamb�m. Basta saber o
 * endere�o IP remoto da m�quina onde o servidor est� rodando e n�o
 * pode haver nenhum firewall no meio do caminho bloqueando conex�es na
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
  /* Os sockets. Um que ser� o socket que vai escutar pelas conex�es
   * e o outro que vai ser o socket espec�fico de cada conex�o */
  int listenfd, listendatafd, connfd, conndatafd;
  socklen_t len;
  /* Informa��es sobre o socket (endere�o e porta) ficam nesta struct */
  struct sockaddr_in server_PI_socket, server_DTP_socket, clientaddr;
  /* Retorno da fun��o fork para saber quem � o processo filho e quem
   * � o processo pai */
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
	
  /* Cria��o de um socket. Eh como se fosse um descritor de arquivo. Eh
   * possivel fazer operacoes como read, write e close. Neste
   * caso o socket criado eh um socket IPv4 (por causa do AF_INET),
   * que vai usar TCP (por causa do SOCK_STREAM), j� que o FTP
   * funciona sobre TCP, e ser� usado para uma aplica��o convencional sobre
   * a Internet (por causa do n�mero 0) */
  if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) == -1)
    {
      perror ("socket :(\n");
      exit (2);
    }
	
  /* Agora � necess�rio informar os endere�os associados a este
   * socket. � necess�rio informar o endere�o / interface e a porta,
   * pois mais adiante o socket ficar� esperando conex�es nesta porta
   * e neste(s) endere�os. Para isso � necess�rio preencher a struct
   * server_PI_socket. � necess�rio colocar l� o tipo de socket (No nosso
   * caso AF_INET porque � IPv4), em qual endere�o / interface ser�o
   * esperadas conex�es (Neste caso em qualquer uma -- INADDR_ANY) e
   * qual a porta. Neste caso ser� a porta que foi passada como
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
	
  /* Como este c�digo � o c�digo de um servidor, o socket ser� um
   * socket passivo. Para isto � necess�rio chamar a fun��o listen
   * que define que este � um socket de servidor que ficar� esperando
   * por conex�es nos endere�os definidos na fun��o bind. */
  if (listen (listenfd, LISTENQ) == -1)
    {
      perror ("listen :(\n");
      exit (4);
    }
	
  printf ("[Servidor no ar. Aguardando conexoes na porta %s]\n",argv[1]);
  printf ("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
	   
  /* O servidor no final das contas � um loop infinito de espera por
   * conex�es e processamento de cada uma individualmente */
  for (;;) {
    len = sizeof (clientaddr);
    /* O socket inicial que foi criado � o socket que vai aguardar
     * pela conex�o na porta especificada. Mas pode ser que existam
     * diversos clientes conectando no servidor. Por isso deve-se
     * utilizar a fun��o accept. Esta fun��o vai retirar uma conex�o
     * da fila de conex�es que foram aceitas no socket listenfd e
     * vai criar um socket espec�fico para esta conex�o. O descritor
     * deste novo socket � o retorno da fun��o accept. */
    if ((connfd = accept (listenfd, (struct sockaddr *) &clientaddr, &len)) == -1 ) {
      perror ("accept :(\n");
      exit (5);
    }
	
    /* Agora o servidor precisa tratar este cliente de forma
     * separada. Para isto � criado um processo filho usando a
     * fun��o fork. O processo vai ser uma c�pia deste. Depois da
     * fun��o fork, os dois processos (pai e filho) estar�o no mesmo
     * ponto do c�digo, mas cada um ter� um PID diferente. Assim �
     * poss�vel diferenciar o que cada processo ter� que fazer. O
     * filho tem que processar a requisi��o do cliente. O pai tem
     * que voltar no loop para continuar aceitando novas conex�es */
    /* Se o retorno da fun��o fork for zero, � porque est� no
     * processo filho. */
    if ( (childpid = fork ()) == 0) {
      /**** PROCESSO FILHO ****/
      printf ("[Uma conexao aberta]\n");
      /* J� que est� no processo filho, n�o precisa mais do socket
       * listenfd. S� o processo pai precisa deste socket. */
      close (listenfd);
	         
      /* Agora pode ler do socket e escrever no socket. Isto tem
       * que ser feito em sincronia com o cliente. N�o faz sentido
       * ler sem ter o que ler. Ou seja, neste caso est� sendo
       * considerado que o cliente vai enviar algo para o servidor.
       * O servidor vai processar o que tiver sido enviado e vai
       * enviar uma resposta para o cliente (Que precisar� estar
       * esperando por esta resposta) 
       */
	
      /* ========================================================= */
      /* ========================================================= */
      /*                         EP1 IN�CIO                        */
      /* ========================================================= */
      /* ========================================================= */
      /* TODO: � esta parte do c�digo que ter� que ser modificada
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

	      /* Socket para transfer�ncia de dados */
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
	
      /* Ap�s ter feito toda a troca de informa��o com o cliente,
       * pode finalizar o processo filho */
      printf("[Uma conexao fechada]\n");
      exit(0);
    }
    /**** PROCESSO PAI ****/
    /* Se for o pai, a �nica coisa a ser feita � fechar o socket
     * connfd (ele � o socket do cliente espec�fico que ser� tratado
     * pelo processo filho) */
    close(connfd);
  }
  exit(0);
}
	
