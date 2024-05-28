#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <stdbool.h>

FILE *file; // inicializujeme soubor pro zapis

sem_t *oxyQue; // semafor pro frontu kysliku
sem_t *hydroQue; // semafor pro frontu vodiku
sem_t *mutex; // semafor pro pristup k tvorbe atomu
sem_t *print_sem; // semafor pro zapis do souboru
sem_t *blocking; // semafor pouzity v bariere
sem_t *blocking2; // semafor2 pouzity v bariere
sem_t *wall; // semafor pro pristup do kriticke sekce

int *oxygen = NULL; // sdilena promenna pro pocitani atomu kysliku
int *hydrogen = NULL; // sdil. prom. pro pocitani atomu vodiku
int *molecule = NULL; // sdil. prom. pro pocitani molekul
int *counter_b = NULL; // sdil. prom. pro pocitani atomu ktere vytvorily molekulu
int *process = NULL; // sdil. prom. pro pocitani poradoveho cisla zapisu do souboru

void closing_sem(void) // funkce pro zavreni semaforu
{
    sem_close(oxyQue);
    sem_close(hydroQue);
    sem_close(print_sem);
    sem_close(mutex);
    sem_close(blocking);
    sem_close(blocking2);
    sem_close(wall);
}

void clean(void) // funkce pro uvolneni dat z pameti
{
// odstranime pojmenovane semafory
sem_unlink("/xniesl00--1");
sem_unlink("/xniesl00--2");
sem_unlink("/xniesl00--3");
sem_unlink("/xniesl00--4");
sem_unlink("/xniesl00--5");
sem_unlink("/xniesl00--6");
sem_unlink("/xniesl00--7");

// dealokujeme sdilene promenne
munmap(oxygen,sizeof(int));
munmap(hydrogen, sizeof(int));
munmap(molecule, sizeof(int));
munmap(counter_b, sizeof(int));
munmap(process, sizeof(int));

// dealokujeme semafory
munmap(print_sem,sizeof(sem_t));
munmap(oxyQue,sizeof(sem_t));
munmap(hydroQue,sizeof(sem_t));
munmap(mutex, sizeof(sem_t));
munmap(blocking, sizeof(sem_t));
munmap(blocking2, sizeof(sem_t));
munmap(wall, sizeof(sem_t));
}

/*
semafor print_sem slouzi pro zapis do souboru, aby nedochazelo ke kolizim
neni proto okomentovany pri kazdem vyskytu v obou funkcich
*/
void process_oxy(int d, int counter, int c_molecule)
{
sem_wait(mutex); // zamkneme pristup dalsim atomum
(*oxygen)++; // inkrementujeme cislo tvoricich se kysliku
sem_wait(print_sem); // zamkne semafor pro zapis do souboru
(*process)++; // inkrementujeme poradove cislo pro zapis
fprintf(file,"%d: O: %d started\n",*process, counter);
sem_post(print_sem); // odemkne semafor pro zapis do souboru
usleep(d); // similuje cas tvorby atomu
sem_wait(print_sem);
(*process)++;
fprintf(file, "%d: O: %d going to queue\n",*process, counter);

if((*hydrogen) > 1) // pokud jsou k dispozici minimalne 2 atomy vodiku ve fronte
{
    sem_post(hydroQue); // odemkne 2 atomy vodiku cekajicich ve fronte
    sem_post(hydroQue);
    (*hydrogen) -=2; // dekrementuje pocitadlo vodiku cekajicich ve fornte

    sem_post(oxyQue); // odemkne 1 atom kysloki cekajiciho ve fronte
    (*oxygen) -=1; // dekrementuje pocitadlo cekajicich kysliku ve fronte
}

else // pokud je k dospozici pouze jeden atom vodiku
{
    sem_post(mutex); // uvolni pristup 1 atomu vodiku do fronty
}
sem_post(print_sem);
sem_wait(oxyQue); // atom kysliku ceka az ho nekdo uvolni tzn. az budou k dispozici 2 atomy vodiku

if((*hydrogen) == 1 && (*oxygen) == 0) // pokud uz nemame k dispozici dostatek atomu pro tvorbu molekuly
{
    fprintf(file, "%d: O: %d not enough H\n",*process, counter);
    
    exit(0); // proces se ukonci
}

//int tmp_molecule = (*molecule); //pomocna lokalni promenna pro vypis cisla molekuly


// zamkne semafor pro pocitani sdil. promenne *counter_b
sem_wait(wall);
(*counter_b)++; // inkrementujeme pocitadlo atomu ktere vytvorily molekulu
if((*counter_b) == 3) // ceka dokud neprojdou 3 atomy potrebne pro vytvoreni molekuly
{
    sem_post(blocking);
    sem_post(blocking);
    sem_post(blocking);
}
sem_post(wall); // odemkne semafor pro pocitani *counter_b
(*molecule)++;
sem_wait(blocking); // procesy cekaji na uvolneni

sem_wait(print_sem);
(*process)++;
fprintf(file,"%d: O: %d creating a molecule %d\n",*process, counter, (*molecule));
usleep(c_molecule); // simuluje cas tvorby molekuly
sem_post(print_sem);

sem_wait(print_sem);
(*process)++;
fprintf(file,"%d: O: %d molecule %d created\n",*process, counter, (*molecule));
sem_post(print_sem);

// zamkne semafor pro pocitani sdil. promenne *counter_b
sem_wait(wall);
(*counter_b)--;
if((*counter_b) == 0) // uvolni 3 procesy ktere vytvorily molekulu
{
    sem_post(blocking2);
    sem_post(blocking2);
    sem_post(blocking2);
}
sem_post(wall); // odemkne semafor pro pocitani *counter_b
sem_wait(blocking2); // procesy cekaji na uvolneni
sem_post(mutex); // otevreme pristup dalsim atomum
fclose(file); // zavreme pristup do souboru
closing_sem(); // zavreme semafory
exit(0); // ukoncime proces
}

void process_hydro(int d, int counter)
{
   
sem_wait(mutex); // zamkneme pristup dalsim atomum
(*hydrogen)++; // inkrementujeme cislo tvoricich se vodiku
sem_wait(print_sem); // uzavreme semafor pro zapis do souboru
(*process)++; // inkrementujeme poradove cislo zapisu do souboru
fprintf(file, "%d: H: %d started\n",*process, counter); // 
sem_post(print_sem); // odemkneme semafor pro zapis do souboru
usleep(d); // simulace tvorby atomu vodiku

sem_wait(print_sem);
(*process)++;
fprintf(file, "%d: H: %d going to queue\n",*process, counter);
sem_post(print_sem);

// pokud jsou ve fronte minimalne 2 atomy vodiku a jeden atom kysliku zacne se tvorit molekula
if((*hydrogen) > 1 && (*oxygen) > 0)
{
 sem_post(hydroQue); // otevre semafor pro 2 atomy vodiku
 sem_post(hydroQue);

 (*hydrogen) -=2; // dekrementuje pocitadlo vodiku cekajicich ve fronte

 sem_post(oxyQue); // otevre semafor pro jeden atom kysliku
 (*oxygen) -=1; // dekrementuje pocitadlo kysliku cekajicich ve fronte
}

else // pokud nejsou k dispozici 2 atomy vodiku
{
    sem_post(mutex); // otevre semafor a ceka na druhy atom vodiku
}

if((*hydrogen) == 1 && (*oxygen) == 0) // kdyz uz nejsou k dispozici zadne atomy kysliku
{
    (*oxygen) = 0;
    (*process)++;
    fprintf(file, "%d: H: %d not enough H or O\n",*process, counter);
    sem_post(oxyQue);
    
    (*hydrogen) -=1; // dekrementuje pocitadlo vodiku ve fronte
    closing_sem(); // zavreme semafory
    fclose(file); // zavreme pristup do souboru
    exit(0); // ukoncime proces
}

// atom vodiku ceka, dokud ho nekdo neuvolni aby mohl zacit tvorbu molekuly
sem_wait(hydroQue);

// zamkne semafor pro pocitani *counter_b
sem_wait(wall);
(*counter_b)++;
if((*counter_b) == 3) // ceka dokud neprojdou 3 atomy ktere vytvorily molekulu
{
    sem_post(blocking);
    sem_post(blocking);
    sem_post(blocking);
}
sem_post(wall);
//odemkne semafor pro pocitani *counter_b

sem_wait(blocking); // atom ceka na uvolneni

sem_wait(print_sem);
(*process)++;
fprintf(file,"%d: H: %d creating a molecule %d\n",*process, counter, *molecule);
sem_post(print_sem);

sem_wait(print_sem);
(*process)++; 
fprintf(file,"%d: H: %d molecule %d created\n",*process, counter, *molecule);
sem_post(print_sem);

// zamkne semafor pro pocitani *counter_b
sem_wait(wall);
(*counter_b)--;
if((*counter_b) == 0) //  uvolni 3 atomy ktere vytvorily molekulu
{
    sem_post(blocking2);
    sem_post(blocking2);
    sem_post(blocking2);
}
sem_post(wall); //odemkne semafor pro pocitani *counter_b
sem_wait(blocking2); // atom ceka na uvolneni
fclose(file); // zavreme pristup do souboru
closing_sem(); // proces zavre semafory
exit(0); // proces se ukonci
}

int main(int argc, char *argv[])
{

if(argc != 5)
{
    fprintf(stderr,"Nespravny pocet argumentu\n");
    return 1;
}

file = fopen("proj2.out", "w"); // otevreme soubor pro zapis

// alokujeme jednotlive sdilene promenne
oxygen = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
hydrogen = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
molecule = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
counter_b = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
process = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);

// alokujeme semafory
print_sem = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
oxyQue = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
hydroQue = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
mutex = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
blocking = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
blocking2 = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);
wall = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, 0, 0);

// inicializujeme jednotlive semafory
oxyQue = sem_open("/xniesl00--1",IPC_CREAT | IPC_EXCL,0666,0);
hydroQue = sem_open("/xniesl00--2",IPC_CREAT | IPC_EXCL,0666,0);
mutex = sem_open("/xniesl00--3",IPC_CREAT | IPC_EXCL,0666,1);
print_sem = sem_open("/xniesl00--4",IPC_CREAT | IPC_EXCL,0666,1);
blocking = sem_open("/xniesl00--5",IPC_CREAT | IPC_EXCL,0666,0);
blocking2 = sem_open("/xniesl00--7",IPC_CREAT | IPC_EXCL,0666,1);
wall = sem_open("/xniesl00--6",IPC_CREAT | IPC_EXCL,0666,1);

// struktura pro argumenty z prikazove radky
    struct Params
    {
        int NO;
        int NH;
        int TI;
        int TB;
    }params;

char *end;    
params.NO = strtoll(argv[1], &end, 10); // pocet atomu kysliku
params.NH = strtoll(argv[2], &end, 10); // pocet atomu vodiku
params.TI = strtoll(argv[3], &end, 10); // delka tvorby atomu v milisekundach
params.TB = strtoll(argv[4], &end, 10); // delka tvorby molekuly v milisekundach

///////////////////////////////////////////////
// chybove stavy argumentu

if(params.TI < 0 || params.TI > 1000)
{
    fprintf(stderr, "Špatně zadaný argument\n");
    closing_sem();
    clean();
    return 1;
}
if(params.TB < 0 || params.TB > 1000)
{
    fprintf(stderr, "Špatně zadaný argument\n");
    closing_sem();
    clean();
    return 1;
}
if(params.NH < 1)
{
    fprintf(stderr, "Špatně zadaný argument\n");
    closing_sem();
    clean();
    return 1;
}
if(params.NO < 1)
{
    fprintf(stderr, "Špatně zadaný argument\n");
    closing_sem();
    clean();
    return 1;
}
////////////////////////////////////////////////
setbuf(file, NULL);

//(*molecule)++; // pocitadlo molekul zacina na 1
int delay; // cas tvorby atomu
int creating_m; // cas tvorby molekuly
int idO = 1; // cislo atomu kysliku (pocitame od 1)
// vytvorime NO procesu kysliku
for(int i = 0; i < params.NO; i++)
{
    pid_t pidO = fork();
    if(pidO == 0)
    {   
        delay = (rand() % ((params.TI) +1) * 1000); // vypocitame cas tvorby atomu - parametr predame funkci
        creating_m = (rand() % ((params.TB) +1) * 1000); // vypocitame cas tvorby molekuly - parametr predame funkci
        process_oxy(delay, idO, creating_m); // detsky proces posleme do funkce pro tvorbu kysliku
        
    }
    if (pidO == -1)
    {
        fprintf(stderr, "chyba funkce fork()\n"); // chybovy stav funkce fork()
        closing_sem(); // zavreme semafory
        clean(); // uvolnime alokovane semafory a sdilene promenne
        return 1;
    }  
    idO++; // inkrementujeme cislo atomu kysliku
    
}

int idH = 1; // cislo atomu vodiku (pocitame od 1)
// vytvorime NH procesu vodiku
for(int i = 0; i < params.NH; i++)
{
    pid_t pidH = fork();
    if(pidH == 0)
    {
        delay = (rand() % ((params.TI) +1) * 1000); // vypocitame cas tvorby atomu - parametr predame funkci
        process_hydro(delay, idH); // detsky proces posleme do funkce pro tvorbu vodiku
        
    }
    if (idH == -1)
    {
        fprintf(stderr, "chyba funkce fork()\n"); // chybovy stav funkce fork()
        closing_sem(); // zavreme semafory
        clean(); // uvolnime alokovane semafory a sdilene promenne
        return 1;
    }  
    idH++; // inkrementujeme cislo atomu vodiku
    
}

while(wait(NULL) > 0); // cekame na ukonceni detskych procesu

closing_sem(); // zavreme semafory
clean(); // uvolnime alokovane semafory a sdilene promenne
fclose(file);
    return 0;
}
