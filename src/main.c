#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* Macro functions */
#define PRINT_SDL_ERR(str) printf(str " error: %s\n", SDL_GetError())
#define PRINT_ARG_CNT_ERR printf("Not enough arguments!\n")

/* SDL flags */
#define SDL_INIT_FLAGS SDL_INIT_VIDEO
#define SDL_RENDERER_FLAGS SDL_RENDERER_ACCELERATED
#define SDL_WINDOW_FLAGS SDL_WINDOW_SHOWN

/* Argument definitions */
#define CLIENT_ARG 1
#define SERVER_IP_CLIENT_ARG 2
#define PORT_CLIENT_ARG 3

#define SERVER_ARG 1
#define PORT_SERVER_ARG 2

#define CLIENT_ARG_CNT 4
#define SERVER_ARG_CNT 3

/* Misc definitions */
#define SDL_KEY_COUNT 322
#define FIRST_PLAYER_ENTITY 0
#define SECOND_PLAYER_ENTITY 1
#define ENTITY_SPEED 1
#define GAME_DELAY 4
#define MAX_PLAYER_CNT 2
#define SOCKET_CHECK_TIMEOUT 10000
#define PLAYER_RECTANGLE_SIZE 16

SDL_Rect g_dstrect;

typedef struct entity_t
{
	char header;
	int x;
	int y;
	SDL_Color color;
}
entity_t;

bool init_sdl
(
	SDL_Window **window,
	SDL_Renderer **renderer
)
{
	bool b_will_init = true;
	
	/* Init SDL and check for errors */
	if(SDL_Init(SDL_INIT_FLAGS) != 0)
	{
		PRINT_SDL_ERR("SDL_Init");
		b_will_init = false;
	}
	
	/* Create the window and check for errors */
	*window = SDL_CreateWindow
	(
		"Networked Demo",
		SDL_WINDOWPOS_UNDEFINED,
		SDL_WINDOWPOS_UNDEFINED,
		640,
		360,
		SDL_WINDOW_FLAGS
	);
	
	if(*window == NULL)
	{
		PRINT_SDL_ERR("SDL_CreateWindow");
		b_will_init = false;
	}
	
	/* Create the renderer and check for errors */
	*renderer = SDL_CreateRenderer
	(
		*window,
		-1,
		SDL_RENDERER_FLAGS
	);
	
	if(*renderer == NULL)
	{
		PRINT_SDL_ERR("SDL_CreateRenderer");
		b_will_init = false;
	}
	
	/* Return */
	return b_will_init;
}

void quit_sdl
(
	SDL_Window *window,
	SDL_Renderer *renderer
)
{
	if(renderer) SDL_DestroyRenderer(renderer);
	if(window) SDL_DestroyWindow(window);
	SDL_Quit();
}

bool init_sdl_net(void)
{
	bool b_will_init = true;

	if(SDLNet_Init() != 0)
	{
		PRINT_SDL_ERR("SDLNet_Init");
		b_will_init = false;
	}
	
	return b_will_init;
}

void quit_sdl_net(void)
{
	SDLNet_Quit();
}

void run_server(int argc, char *argv[])
{
	/* Define variables */
	bool b_is_running;
	char *p_data_buf;
	entity_t dummy_entity;
	entity_t entities[MAX_PLAYER_CNT];
	int port;
	int connected_client_cnt;
	int ready_socket_cnt;
	IPaddress server_ip;
	SDLNet_SocketSet socket_set;
	TCPsocket client_socket[MAX_PLAYER_CNT];
	TCPsocket server_socket;
	
	
	/* Set initial variables */
	b_is_running = true;
	p_data_buf = NULL;
	p_data_buf = malloc(MAX_PLAYER_CNT * sizeof(entity_t));
	connected_client_cnt = 0;
	ready_socket_cnt = 0;
	for(int i = 0; i < MAX_PLAYER_CNT; i++) client_socket[i] = NULL;
	
	/* Dummy entity setup */
	dummy_entity.header = 'h';
	dummy_entity.x = 0;
	dummy_entity.y = 0;
	dummy_entity.color.r = rand() % 256;
	dummy_entity.color.g = rand() % 256;
	dummy_entity.color.b = rand() % 256;
	dummy_entity.color.a = 255;
	
	/* Init SDL and SDL_net */
	if(SDL_Init(0) != 0)
	{
		PRINT_SDL_ERR("SDL_Init");
		b_is_running = false;
	}
	
	b_is_running = init_sdl_net();
	
	/* Check for sufficient arg count */
	if(argc < SERVER_ARG_CNT)
	{
		PRINT_ARG_CNT_ERR;
		b_is_running = false;
	}
	
	/* Initialize the server */
	if(b_is_running)
	{	
		/*
		 * Allocate the socket set (added 1 to MAX_PLAYER_CNT to
		 * account for the server socket on top of the two player
		 * sockets)
		 */
		socket_set = SDLNet_AllocSocketSet(MAX_PLAYER_CNT + 1);
		
		/*
		 * Get the server port, resolve the host, and add the socket to
		 * the socket set
		 */
		port = atoi(argv[PORT_SERVER_ARG]);	
		SDLNet_ResolveHost(&server_ip, NULL, port);
		server_socket = SDLNet_TCP_Open(&server_ip);
		SDLNet_TCP_AddSocket(socket_set, server_socket);
		
		/* Check to see if the port is valid */
		if(port < 1)
		{
			printf("Invalid port number!\n");
			b_is_running = false;
		}
		
		/* Run the main loop */
		while(b_is_running)
		{
			/* Check for socket activity */
			ready_socket_cnt = SDLNet_CheckSockets(socket_set, SOCKET_CHECK_TIMEOUT);
			printf("Checking for socket activity...\n");
			if(ready_socket_cnt > 0) for(int i = 0; i < MAX_PLAYER_CNT; i++)
			{
				printf("Found socket activity!\n");
				printf("Socket activity count of %d\n", ready_socket_cnt);
				
				/* Connect the client if it isn't already */
				if(SDLNet_SocketReady(socket_set) && !client_socket[i])
				{
					printf("Socket ready for connection...\n");
										
					/* Accept the client socket */	
					client_socket[i] = SDLNet_TCP_Accept(server_socket);
					
					if(client_socket[i])
					{
						/* Add the socket to the socket set */
						SDLNet_TCP_AddSocket(socket_set, client_socket[i]);
						printf("Client connected and added to socket set!\n");
						
						/* Display number of connected clients */
						connected_client_cnt += 1;
						printf("%d client(s) connected\n", connected_client_cnt);
					}
					else
					{
						PRINT_SDL_ERR("SDLNet_TCP_Accept");
						SDLNet_TCP_Close(client_socket[i]);
					}
				}
				else if(SDLNet_SocketReady(client_socket[i]))
				{
					/* Clear the data buffer */
					memset(p_data_buf, 0, sizeof(entity_t));
				
					/* Receive the client data */
					SDLNet_TCP_Recv(client_socket[i], p_data_buf, sizeof(entity_t));
					
					/* Send the data to all other clients */
					for(int j = 0; j < MAX_PLAYER_CNT; j++)
					{
						/*
						 * XXX: NOTE: This will probably only work with
						 * two total clients at the moment.  It's
						 * Essentially just swapping entity data
						 * between two clients.  Any more will likely
						 * break it
						 */
						
						if(i != j && client_socket[j] == NULL)
						{
							/*
							 * Send dummy entity data if there is no
							 * other client.  This is to prevent
							 * freezing on the clients' end if there
							 * are no other users connected.  There's
							 * probably a better way to do this, but it
							 * works for a proof-of-concept.
							 */
/*							memcpy(p_data_buf, &dummy_entity, sizeof(entity_t));*/
/*							*/
/*							SDLNet_TCP_Send*/
/*							(*/
/*								client_socket[j],*/
/*								&dummy_entity,*/
/*								sizeof(entity_t)*/
/*							);*/

							/* TODO: It keeps segfaulting for some reason... */
						}
						else if(i != j)
						{	
							/* Otherwise, send the other client's entity data */
							SDLNet_TCP_Send
							(
								client_socket[j],
								p_data_buf,
								sizeof(entity_t)
							);
						}
					}
					
					/* Disconnect the client if the message is empty */
					if(strcmp(p_data_buf, "") == 0)
					{
						/* Close the connection */
						SDLNet_TCP_DelSocket(socket_set, client_socket[i]);
						SDLNet_TCP_Close(client_socket[i]);
						client_socket[i] = NULL;
						printf("Client disconnected and removed from socket set!\n");
						
						/* Update the connected client count */
						connected_client_cnt -= 1;
						printf("%d client(s) connected\n", connected_client_cnt);
					}
				}
			}
		}
		
		/* Delete server socket*/
		if(server_socket)
		{
			SDLNet_TCP_DelSocket(socket_set, server_socket);
		}
		
		/* Free the socket set */
		SDLNet_FreeSocketSet(socket_set);
	}
	
	/* Free allocated memory */
	if(p_data_buf) free(p_data_buf);
	
	/* Quit SDL and SDL_net */
	quit_sdl_net();
	SDL_Quit();
}

void run_client(int argc, char *argv[])
{
	/* Define variables*/
	bool b_is_connected;
	bool b_is_running;
	bool b_key_presses[SDL_KEY_COUNT];
	char *p_data_buf;
	entity_t entities[MAX_PLAYER_CNT];
	int port;
	IPaddress server_ip;
	SDL_Event event;
	SDL_Renderer *renderer;
	SDL_Window *window;
	TCPsocket socket;
	
	/* Set initial variables */
	b_is_connected = false;
	b_is_running = true;
	p_data_buf = NULL;
	p_data_buf = malloc(MAX_PLAYER_CNT * sizeof(entity_t));
	memset(b_key_presses, false, SDL_KEY_COUNT);
	renderer = NULL;
	window = NULL;
	
	/* Init SDL and SDL_net */
	b_is_running = init_sdl(&window, &renderer);
	b_is_running = init_sdl_net();
	
	/* Seed random number generation */
	srand(time(0));
	
	/* Set the players' initial states */
	entities[FIRST_PLAYER_ENTITY].header = 'h';
	entities[FIRST_PLAYER_ENTITY].x = 0;
	entities[FIRST_PLAYER_ENTITY].y = 0;
	entities[FIRST_PLAYER_ENTITY].color.r = rand() % 256;
	entities[FIRST_PLAYER_ENTITY].color.g = rand() % 256;
	entities[FIRST_PLAYER_ENTITY].color.b = rand() % 256;
	entities[FIRST_PLAYER_ENTITY].color.a = 255;
	
	memcpy(&entities[SECOND_PLAYER_ENTITY], &entities[FIRST_PLAYER_ENTITY], sizeof(entity_t));
	
	/* Set the player rectangle width and height */
	g_dstrect.w = PLAYER_RECTANGLE_SIZE;
	g_dstrect.h = PLAYER_RECTANGLE_SIZE;
	
	/* Check for sufficient arg count */
	if(argc < CLIENT_ARG_CNT)
	{
		PRINT_ARG_CNT_ERR;
		b_is_running = false;
	}
	
	if(b_is_running)
	{
		/* Get the server port and resolve the host */
		port = atoi(argv[PORT_CLIENT_ARG]);	
		SDLNet_ResolveHost(&server_ip, argv[SERVER_IP_CLIENT_ARG], port);
		socket = SDLNet_TCP_Open(&server_ip);
	
		/* Run main game loop */
		while(b_is_running)
		{
			if(socket)
			{
				b_is_connected = true;
			}
			else
			{
				b_is_connected = false;
			}
		
			if(b_is_connected)
			{
				memcpy(p_data_buf, &entities[FIRST_PLAYER_ENTITY], sizeof(entity_t));
				SDLNet_TCP_Send(socket, p_data_buf, sizeof(entity_t));
				SDLNet_TCP_Recv(socket, p_data_buf, sizeof(entity_t));
				memcpy(&entities[SECOND_PLAYER_ENTITY], p_data_buf, sizeof(entity_t));
			}
		
			/* Handle SDL events */
			while(SDL_PollEvent(&event)) switch(event.type)
			{
				case SDL_QUIT:
					b_is_running = false;
					break;
					
				case SDL_KEYDOWN:
					b_key_presses[event.key.keysym.scancode] = true;
					break;
				
				case SDL_KEYUP:
					b_key_presses[event.key.keysym.scancode] = false;
					break;
			}
			
			/* Handle controls */
			if(b_key_presses[SDL_SCANCODE_ESCAPE])
				b_is_running = false;
			if(b_key_presses[SDL_SCANCODE_LEFT])
				entities[FIRST_PLAYER_ENTITY].x -= ENTITY_SPEED;
			if(b_key_presses[SDL_SCANCODE_UP])
				entities[FIRST_PLAYER_ENTITY].y += ENTITY_SPEED;
			if(b_key_presses[SDL_SCANCODE_DOWN])
				entities[FIRST_PLAYER_ENTITY].y -= ENTITY_SPEED;
			if(b_key_presses[SDL_SCANCODE_RIGHT])
				entities[FIRST_PLAYER_ENTITY].x += ENTITY_SPEED;
			
			/* Render game */
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
			SDL_RenderClear(renderer);
			
			for(int i = 0; i < MAX_PLAYER_CNT; i++)
			{
				SDL_SetRenderDrawColor
				(
					renderer,
					entities[i].color.r,
					entities[i].color.g,
					entities[i].color.b,
					entities[i].color.a 
				);
				
				g_dstrect.x = entities[i].x;
				g_dstrect.y = -1 * entities[i].y;
				SDL_RenderFillRect(renderer, &g_dstrect);
			}
			
			SDL_RenderPresent(renderer);
			
			SDL_Delay(GAME_DELAY);
		}
		
		/* Close the connection */
		SDLNet_TCP_Close(socket);
	}
	
	/* Free memory */
	if(p_data_buf) free(p_data_buf);
	
	/* Quit SDL and SDL_net */
	quit_sdl_net();
	quit_sdl(window, renderer);
}

int main(int argc, char *argv[])
{
	
	if(argc < 2)
	{
		PRINT_ARG_CNT_ERR;
	}
	else if(strcmp(argv[CLIENT_ARG], "client") == 0)
	{
		run_client(argc, argv);
	}
	else if(strcmp(argv[SERVER_ARG], "server") == 0)
	{
		run_server(argc, argv);
	}
	else
	{
		printf("Usage: `main client [server ip] [port]` OR `main server [port]`\n");
	}
	
	/* Return */
	return 0;
}
