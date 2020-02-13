int sh_execute(char **args, struct sigaction sa);

int sh_launch(char **args, struct sigaction sa);

char** sh_split_line(char* line);

char* sh_read_line();

void sh_handle_background_process(char **args, int status);

void sh_handle_redirect(char **args);