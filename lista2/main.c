#include <stdio.h>
#include <string.h>

enum State{
    CODE,
    SLASH,
    ASTERISK,
    COMMENT
} state;

int main(int argc, char **argv){
    FILE *arq;
    FILE *new_arq;
    char arq_name[30];
    char c;
    state = CODE;

    if(argc == 2){
        if((arq = fopen(argv[1], "r")) == NULL){
            perror("Error ");
            return(1);
        }
        if((new_arq = fopen("no_comment.c", "w")) == NULL){
            perror("Error ");
            return(1);
        }
    }
    else{
        printf("Digite o nome do arquivo com o seu diretorio: \n");
        fflush(stdin);
        fgets(arq_name, 29, stdin);
        if(arq_name[strlen(arq_name) - 1] == '\n'){
            arq_name[strlen(arq_name) - 1] = '\0';
        }
    
        if((arq = fopen(arq_name, "r")) == NULL){
            perror("Error ");
            return(1);
        }
        if((new_arq = fopen("no_comment.c", "w")) == NULL){
            perror("Error ");
            return(1);
        }
    }
    
    while ((c = fgetc(arq)) != EOF) {
        switch(state){
            case CODE:
                if(c == '/'){
                    state = SLASH;
                }
                else{
                    state = CODE;
                    fputc(c, new_arq);
                }
                break;

            case SLASH:
                if(c == '*'){
                    state = COMMENT;
                }
                else {
                    state = CODE;
                    fputc('/', new_arq);
                    fputc(c, new_arq);
                }
                break;

            case COMMENT:
                if(c == '*'){
                    state = ASTERISK;
                }
                else{
                    state = COMMENT;
                }
                break;

            case ASTERISK:
                if(c == '/'){
                    state = CODE;
                }
                else{
                    state = ASTERISK;
                }
                break;
        }
    }

    if(state == SLASH){
        fputc('/', new_arq);
    }

    fclose(arq);
    fclose(new_arq);
}