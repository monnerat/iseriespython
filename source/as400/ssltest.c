/* GSK Client Program using Application Id */
/* This program assumes that the application id is */
/* already registered and a certificate has been */
/* associated with the application id */
/* */
/* No parameters, some comments and many hardcoded *
/* values to keep it short and simple */
/* use following command to create bound program: */
/* CRTBNDC PGM(MYLIB/GSKCLIENT) */
/* SRCFILE(MYLIB/CSRC) */
/* SRCMBR(GSKCLIENT) */
#include <stdio.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <gskssl.h> 
#include <netinet/in.h> 
#include <arpa/inet.h> 
#include <errno.h> 
#define TRUE 1 
#define FALSE 0 
#pragma datamodel(P128)
void main(void)
{
    gsk_handle my_env_handle=NULL; /* secure environment handle */ 
    gsk_handle my_session_handle=NULL; /* secure session handle */ 
    struct sockaddr_in address;
    int buf_len, rc = 0, sd = -1;
    int amtWritten, amtRead;
    char buff1[1024];
    char buff2[1024]; /* hardcoded IP address (change to make address where server program runs) */
#pragma convert(37)
    char addr[16] = "129.142.220.37"; /*********************************************/
#pragma convert(0)
    /* Issue all of the command in a do/while */
    /* loop so that cleanup can happen at end */
    /*********************************************/
    do { /* open a gsk environment */ rc = errno = 0;
        rc = gsk_environment_open(&my_env_handle);
        if (rc != GSK_OK) { printf("gsk_environment_open() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* set the Application ID to use */
        rc = errno = 0;

#pragma convert(37)
        rc = gsk_attribute_set_buffer(my_env_handle, GSK_OS400_APPLICATION_ID, "PYTHON", 6);
#pragma convert(0)
        if (rc != GSK_OK) { printf("gsk_attribute_set_buffer() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* set this side as the client (this is the default */
        rc = errno = 0;
        rc = gsk_attribute_set_enum(my_env_handle, GSK_SESSION_TYPE, GSK_CLIENT_SESSION);
        if (rc != GSK_OK) { printf("gsk_attribute_set_enum() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* by default SSL_V2, SSL_V3, and TLS_V1 are enabled */
        /* We will disable SSL_V2 for this example. */
        /*rc = errno = 0;
        rc = gsk_attribute_set_enum(my_env_handle, GSK_PROTOCOL_SSLV2, GSK_PROTOCOL_SSLV2_OFF);
        if (rc != GSK_OK) { printf("gsk_attribute_set_enum() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }*/
        /* set the cipher suite to use. By default our default list */
        /* of ciphers is enabled. For this example we will just use one */
        /*rc = errno = 0;
        rc = gsk_attribute_set_buffer(my_env_handle, GSK_V3_CIPHER_SPECS, "05", 2);
        if (rc != GSK_OK) { printf("gsk_attribute_set_buffer() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }*/
        /* Initialize the secure environment */
        rc = errno = 0;
        rc = gsk_environment_init(my_env_handle);
        if (rc != GSK_OK) { printf("gsk_environment_init() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* initialize a socket to be used for listening */
        sd = socket(AF_INET, SOCK_STREAM, 0);
        if (sd < 0) { perror("socket() failed");
            break;
        }
        /* connect to the server using a set port number */
        memset((char *) &address, 0, sizeof(address));
        address.sin_family = AF_INET;
        address.sin_port = 443;
        address.sin_addr.s_addr = inet_addr(addr);
        rc = connect(sd, (struct sockaddr *) &address, sizeof(address));
        if (rc < 0) { perror("connect() failed");
            break;
        }
        /* open a secure session */
        rc = errno = 0;
        rc = gsk_secure_soc_open(my_env_handle, &my_session_handle);
        if (rc != GSK_OK) { printf("gsk_secure_soc_open() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* associate our socket with the secure session */
        rc=errno=0;
        rc = gsk_attribute_set_numeric_value(my_session_handle, GSK_FD, sd);
        if (rc != GSK_OK) {
            printf("gsk_attribute_set_numeric_value() failed with rc = %d ", rc);
            printf("and errno = %d.\n", errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* initiate the SSL handshake */
        rc = errno = 0;
        rc = gsk_secure_soc_init(my_session_handle);
        if (rc != GSK_OK) 
        { 
#pragma convert(37)
            /*printf("gsk_secure_soc_init() failed with rc = %d and errno = %d.\n", rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));*/
#pragma convert(0)
            break;
        }
        /* memset buffer to hex zeros */
        memset((char *) buff1, 0, sizeof(buff1));
        /* send a message to the server using the secure session */
        strcpy(buff1,"Test of gsk_secure_soc_write \n\n");
        /* send the message to the client using the secure session */
        buf_len = strlen(buff1);
        amtWritten = 0;
        rc = gsk_secure_soc_write(my_session_handle, buff1, buf_len, &amtWritten);
        if (amtWritten != buf_len) 
        {
            if (rc != GSK_OK)
            {
                printf("gsk_secure_soc_write() rc = %d and errno = %d.\n",rc,errno);
                printf("rc of %d means %s\n", rc, gsk_strerror(rc));
                break;
            } else {
                printf("gsk_secure_soc_write() did not write all data.\n");
                break;
            }
        }
        /* write results to screen */
        printf("gsk_secure_soc_write() wrote %d bytes...\n", amtWritten);
        printf("%s\n",buff1);
        /* memset buffer to hex zeros */
        memset((char *) buff2, 0x00, sizeof(buff2));
        /* receive a message from the client using the secure session */
        amtRead = 0;
        rc = gsk_secure_soc_read(my_session_handle, buff2, sizeof(buff2), &amtRead);
        if (rc != GSK_OK) { printf("gsk_secure_soc_read() rc = %d and errno = %d.\n",rc,errno);
            printf("rc of %d means %s\n", rc, gsk_strerror(rc));
            break;
        }
        /* write results to screen */
        printf("gsk_secure_soc_read() received %d bytes, here they are ...\n", amtRead);
        printf("%s\n",buff2);
    } while(FALSE);
    /* disable SSL support for the socket */
    if (my_session_handle != NULL) gsk_secure_soc_close(&my_session_handle);
    /* disable the SSL environment */
    if (my_env_handle != NULL) gsk_environment_close(&my_env_handle);
    /* close the connection */
    if (sd > -1) close(sd);
    return;
}
#pragma datamodel(pop)

