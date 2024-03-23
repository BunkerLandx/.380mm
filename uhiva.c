#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <tchar.h>
#include <fcntl.h>
#include <process.h>

int child_pid=0;

int fail(char *format, char *data) {
    /* Imprime mensaje de error en stderr y devuelve 2 */
    fprintf(stderr, format, data);
    return 2;
}

char *quoted(char *data) {
    int i, ln = strlen(data), nb;

    /* Asignamos el doble de espacio necesario para manejar el peor caso
       de tener que escapar todo. */
    char *result = calloc(ln*2+3, sizeof(char));
    char *presult = result;

    *presult++ = '"';
    for (nb=0, i=0; i < ln; i++)
      {
        if (data[i] == '\\')
          nb += 1;
        else if (data[i] == '"')
          {
            for (; nb > 0; nb--)
              *presult++ = '\\';
            *presult++ = '\\';
          }
        else
          nb = 0;
        *presult++ = data[i];
      }

    for (; nb > 0; nb--)        /* Manejar las barras diagonales finales */
      *presult++ = '\\';

    *presult++ = '"';
    *presult++ = 0;
    return result;
}

char *loadable_exe(char *exename) {
    /* HINSTANCE hPython;  Manejador DLL para el ejecutable de Python */
    char *result;

    /* hPython = LoadLibraryEx(exename, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!hPython) return NULL; */

    /* Devuelve el nombre de archivo absoluto para spawnv */
    result = calloc(MAX_PATH, sizeof(char));
    strncpy(result, exename, MAX_PATH);
    /*if (result) GetModuleFileNameA(hPython, result, MAX_PATH);

    FreeLibrary(hPython); */
    return result;
}


char *find_exe(char *exename, char *script) {
    char drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
    char path[_MAX_PATH], c, *result;

    /* Convertir barras a barras diagonales para una búsqueda uniforme a continuación */
    result = exename;
    while (c = *result++) if (c=='/') result[-1] = '\\';

    _splitpath(exename, drive, dir, fname, ext);
    if (drive[0] || dir[0]=='\\') {
        return loadable_exe(exename);   /* Ruta absoluta, usar directamente */
    }
    /* Usar el directorio padre del script, que debería ser el directorio principal de Python
       (Esto solo debería usarse para scripts instalados con bdist_wininst, porque
        los scripts instalados con easy_install usan la ruta absoluta a python[w].exe
    */
    _splitpath(script, drive, dir, fname, ext);
    result = dir + strlen(dir) -1;
    if (*result == '\\') result--;
    while (*result != '\\' && result>=dir) *result-- = 0;
    _makepath(path, drive, dir, exename, NULL);
    return loadable_exe(path);
}


char **parse_argv(char *cmdline, int *argc)
{
    /* Analiza una línea de comandos en su lugar usando las reglas de MS C */

    char **result = calloc(strlen(cmdline), sizeof(char *));
    char *output = cmdline;
    char c;
    int nb = 0;
    int iq = 0;
    *argc =  0;

    result[0] = output;
    while (isspace(*cmdline)) cmdline++;   /* Saltar espacios iniciales */

    do {
        c = *cmdline++;
        if (!c || (isspace(c) && !iq)) {
            while (nb) {*output++ = '\\'; nb--; }
            *output++ = 0;
            result[++*argc] = output;
            if (!c) return result;
            while (isspace(*cmdline)) cmdline++;  /* Saltar espacios iniciales */
            if (!*cmdline) return result;  /* Evitar argumento vacío si hay espacios finales */
            continue;
        }
        if (c == '\\')
            ++nb;   /* Contar \'s */
        else {
            if (c == '"') {
                if (!(nb & 1)) { iq = !iq; c = 0; }  /* Saltar " a menos que haya un número impar de \ */
                nb = nb >> 1;   /* Dividir a la mitad los \'s */
            }
            while (nb) {*output++ = '\\'; nb--; }
            if (c) *output++ = c;
        }
    } while (1);
}

void pass_control_to_child(DWORD control_type) {
    /*
     * distribute-issue207
     * pasa el evento de control al proceso secundario (Python)
     */
    if (!child_pid) {
        return;
    }
    GenerateConsoleCtrlEvent(child_pid,0);
}

BOOL control_handler(DWORD control_type) {
    /*
     * distribute-issue207
     * función de devolución de llamada del controlador de eventos de control
     */
    switch (control_type) {
        case CTRL_C_EVENT:
            pass_control_to_child(0);
            break;
    }
    return TRUE;
}

int create_and_wait_for_subprocess(char* command) {
    /*
     * distribute-issue207
     * lanza el proceso secundario (Python)
     */
    DWORD return_value = 0;
    LPSTR commandline = command;
    STARTUPINFOA s_info;
    PROCESS_INFORMATION p_info;
    ZeroMemory(&p_info, sizeof(p_info));
    ZeroMemory(&s_info, sizeof(s_info));
    s_info.cb = sizeof(STARTUPINFO);
    // Configurar la función de devolución de llamada del controlador de eventos
    SetConsoleCtrlHandler((PHANDLER_ROUTINE) control_handler, TRUE);
    if (!CreateProcessA(NULL, commandline, NULL, NULL, TRUE, 0, NULL, NULL, &s_info, &p_info)) {
        fprintf(stderr, "Error al crear el proceso.\n");
        return 0;
    }
    child_pid = p_info.dwProcessId;
    // Esperar a que Python termine
    WaitForSingleObject(p_info.hProcess, INFINITE);
    if (!GetExitCodeProcess(p_info.hProcess, &return_value)) {
        fprintf(stderr, "Error al obtener el código de salida del proceso.\n");
        return 0;
    }
    return return_value;
}

char* join_executable_and_args(char *executable, char **args, int argc)
{
    /*
     * distribute-issue207
     * CreateProcess necesita una cadena larga con el ejecutable y los argumentos de la línea de comandos,
     * por lo que necesitamos convertirla a partir de los argumentos que se construyeron
     */
    int len,counter;
    char* cmdline;

    len=strlen(executable)+2;
    for (counter=1; counter<argc; counter++) {
        len+=strlen(args[counter])+1;
    }

    cmdline = (char*)calloc(len, sizeof(char));
    sprintf(cmdline, "%s",char* join_executable_and_args(char *executable, char **args, int argc)
{
    /*
     * distribute-issue207
     * CreateProcess necesita una cadena larga con el ejecutable y los argumentos de la línea de comandos,
     * por lo que necesitamos convertirla a partir de los argumentos que se construyeron
     */
    int len, counter;
    char* cmdline;

    len = strlen(executable) + 2;
    for (counter = 1; counter < argc; counter++) {
        len += strlen(args[counter]) + 1;
    }

    cmdline = (char*)calloc(len, sizeof(char));
    sprintf(cmdline, "%s", executable);

    for (counter = 1; counter < argc; counter++) {
        strcat(cmdline, " ");
        strcat(cmdline, args[counter]);
    }

    return cmdline;
}
