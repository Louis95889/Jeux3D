NAME    = cub3d
CC      = gcc
CFLAGS  = -Wall -Wextra -Werror -I./include
LDFLAGS = $(shell sdl2-config --libs) -lSDL2_image -lSDL2_mixer -lSDL2_ttf -lm

SRCS    = src/main.c src/init.c src/worldgen.c src/map.c src/render.c \
          src/events.c src/keybind.c \
          src/textures.c src/hud.c src/player3d.c \
          src/objects.c src/obj_loader.c
OBJS    = $(SRCS:.c=.o)

all: $(NAME)

$(NAME): $(OBJS)
	$(CC) $(OBJS) -o $(NAME) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) $(shell sdl2-config --cflags) -c $< -o $@

clean:
	rm -f $(OBJS)

fclean: clean
	rm -f $(NAME)

re: fclean all

.PHONY: all clean fclean re
