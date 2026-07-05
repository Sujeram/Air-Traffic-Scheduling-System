#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <windows.h>
#include <math.h>
#include <mmsystem.h>

#define MAX_FLIGHTS 50
#define MAX_RUNWAYS 3

typedef struct {
    char flightID[10];
    char fNAME[20];
    char ORIGIN[30];
    char DESTINATION[30];
    int departureHour;
    int departureMinute;
    int arrivalHour;
    int arrivalMinute;
    char RUNWAY;
    int PRIORITY;  // 1: emergency, 2: international, 3: domestic
} Flight;
int Delayed[3]={0,0,0};
typedef struct {
    char ID;
    int LastOccupied;
} Runway;

Flight flights[MAX_FLIGHTS];
int flightCount = 0;

Runway runways[] = {
    {'A', 0},
    {'B', 0},
    {'C', 0}
};

char Place[30];
char date[10];
MYSQL *conn = NULL;

int minutes(int hours, int minutes) {
    return (hours * 60) + minutes;
}
void playAlertSound(){
    Beep(750,300);
}
void addFlight();
char assignRunway(int hour, int min);
void AddToDB(char flightID[], char fNAME[], char ORIGIN[], char DESTINATION[], int departureHour, int departureMinute, int arrivalHour, int arrivalMinute, int PRIORITY, char date[], char RUNWAY);
void displaySchedule(char date[],char Place[]);
void assignCrew(char flightID[], char date[],int time);
void delayFlight(int *hour, int *minute, char runwayID);
void conflictResolution(char flightID[], int departureHour, int departureMinute, int arrivalHour, int arrivalMinute, int priority, char date[]);
void emergency();
void generateRunwayReport(char date[], char airport[]);
void reassignCrewAfterCancellations(char date[]);
void allotCrew();
int isRunwayOccupied(char runwayID, int time, char Place[]);

#pragma comment(lib, "winmm.lib")

int main() {
    int choice;
    conn = mysql_init(NULL);
    if (conn == NULL) {
        fprintf(stderr, "mysql_init() failed\n");
        return EXIT_FAILURE;
    }
    printf("MySQL client initialized.\n");
    if (mysql_real_connect(conn, "localhost", "root", "root", "ATSS", 0, NULL, 0) == NULL) {
        fprintf(stderr, "mysql_real_connect() failed: %s\n", mysql_error(conn));
        mysql_close(conn);
        return EXIT_FAILURE;
    }
    printf("\nEnter Today's Date (YYYY-MM-DD): ");
    scanf("%s", date);
    printf("\nEnter airport NAME: ");
    scanf("%s", Place);
    const char *soundFile = "international-airport-intercom-madeira-19947.wav";
    PlaySound(TEXT(soundFile), NULL, SND_FILENAME | SND_ASYNC);
    while (1) {
        printf("\nAir Traffic Scheduling System\n");
        printf("1. Add Flight\n");
        printf("2. Display Schedule\n");
        printf("3.Assign crew\n");
        printf("4.Emergency \n");
        printf("5.Runway report\n");
        printf("0. Exit\n");
        printf("Enter your choice: ");
        scanf("%d", &choice);
        switch (choice) {
            case 1:
                addFlight();
                break;
            case 2:
                displaySchedule(date,Place);
                break;
            case 3:
                allotCrew();
                break;
            case 4:
                emergency();
                break;
            case 5:
                generateRunwayReport(date,Place);
                break;
            case 0:
                printf("Exiting the program.\n");
                mysql_close(conn);
                exit(0);
            default:
                printf("Invalid choice. Please try again.\n");
        }
        
    }
    Sleep(30000);
    return 0;
}

void addFlight() {
    if (flightCount >= MAX_FLIGHTS) {
        printf("Cannot add more flights. Maximum limit reached.\n");
        return;
    }
    printf("Enter Flight ID: ");
    scanf("%s", flights[flightCount].flightID);

    printf("Enter flight NAME: ");
    scanf(" %[^\n]", flights[flightCount].fNAME);

    printf("Enter ORIGIN: ");
    scanf("%s", flights[flightCount].ORIGIN);

    printf("Enter DESTINATION: ");
    scanf("%s", flights[flightCount].DESTINATION);

    printf("Scheduled Departure (HH:MM): ");
    scanf("%d:%d", &flights[flightCount].departureHour, &flights[flightCount].departureMinute);

    printf("Scheduled Arrival (HH:MM): ");
    scanf("%d:%d", &flights[flightCount].arrivalHour, &flights[flightCount].arrivalMinute);

    printf("PRIORITY (1: Emergency, 2: International, 3: Domestic): ");
    scanf("%d", &flights[flightCount].PRIORITY);
    if (strcmp(flights[flightCount].ORIGIN, flights[flightCount].DESTINATION) != 0 && (fabs(strcmp(flights[flightCount].ORIGIN,Place))!=fabs(strcmp(flights[flightCount].DESTINATION,Place)))&&
        minutes(flights[flightCount].departureHour, flights[flightCount].departureMinute) !=
        minutes(flights[flightCount].arrivalHour, flights[flightCount].arrivalMinute)) {
            char Rid;
            int i= strcmp(flights[flightCount].DESTINATION,Place);
            if(i){
                Rid=assignRunway(flights[flightCount].departureHour,flights[flightCount].departureMinute);
            }
            else{
                Rid=assignRunway(flights[flightCount].arrivalHour,flights[flightCount].arrivalMinute);
            }
            if(Rid!='X'){
                flights[flightCount].RUNWAY=Rid;
            }
            else{
                conflictResolution(flights[flightCount].flightID, flights[flightCount].departureHour, flights[flightCount].departureMinute, flights[flightCount].arrivalHour, flights[flightCount].arrivalMinute, flights[flightCount].PRIORITY, date);
            }
  
        AddToDB(flights[flightCount].flightID, flights[flightCount].fNAME, flights[flightCount].ORIGIN,
                flights[flightCount].DESTINATION, flights[flightCount].departureHour, flights[flightCount].departureMinute,
                flights[flightCount].arrivalHour, flights[flightCount].arrivalMinute, flights[flightCount].PRIORITY, 
                date, flights[flightCount].RUNWAY);

        flightCount++;
        printf("Flight added successfully!\n");
    } else {
        printf("Invalid DEPARTURE AND ARRIVAL inputs \n");
    }
}

char assignRunway(int hour, int min) {
    int currTime = minutes(hour, min);

    for (int i = 0; i < sizeof(runways)/sizeof(runways[0]); i++) {
        if (currTime - runways[i].LastOccupied >= 15) {
            if (!isRunwayOccupied(runways[i].ID, currTime,Place)) {
                runways[i].LastOccupied = currTime;
                return runways[i].ID;
            }
        }
    }
    return 'X';//NO RUNWAY ASSIGNED
}

int isRunwayOccupied(char runwayID, int time, char Place[]) {
    char query[512];
    sprintf(query, 
        "SELECT COUNT(*) FROM FLIGHTS WHERE RUNWAY = '%c' "
        "AND ((DEPARTTIME <= '%02d:%02d' AND ARRIVALTIME > '%02d:%02d')) "
        "AND (ORIGIN = '%s' OR DESTINATION = '%s')",
        runwayID, time / 60, time % 60, time / 60, time % 60, Place, Place);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error executing query to check runway occupancy: %s\n", mysql_error(conn));
        return -1;  
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
        return -1;  
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    if (row) {
        int count = atoi(row[0]);  // Convert the count to an integer
        mysql_free_result(res);   
        return count;             
    } else {
        mysql_free_result(res);   
        return 0;                 
    }
}
void AddToDB(char flightID[], char fNAME[], char ORIGIN[], char DESTINATION[], 
    int departureHour, int departureMinute, int arrivalHour, int arrivalMinute, 
    int PRIORITY, char date[], char RUNWAY) {

char query[512];
sprintf(query,
"INSERT INTO FLIGHTS (FID, FNAME, ORIGIN, DESTINATION, DEPARTTIME, ARRIVALTIME, PRIORITY, DATEOFFLIGHT, RUNWAY) "
"VALUES ('%s', '%s', '%s', '%s', '%02d:%02d', '%02d:%02d', %d, '%s', '%c')",
flightID, fNAME, ORIGIN, DESTINATION, departureHour, departureMinute, 
arrivalHour, arrivalMinute, PRIORITY, date, RUNWAY);

assignCrew(flightID,  date,minutes(departureHour,departureMinute));

if (mysql_query(conn, query)) {
fprintf(stderr, "Error executing query: %s\n", mysql_error(conn));
return;
} else {
printf("Flight successfully added to database.\n");
}
}

void displaySchedule(char date[],char Place[]) {
    char query[512];
sprintf(query,
    "SELECT F.FID, F.FNAME, F.ORIGIN, F.DESTINATION, F.DEPARTTIME, F.ARRIVALTIME, F.PRIORITY, F.RUNWAY, GROUP_CONCAT(C.Name SEPARATOR ', ') AS CrewMembers "
    "FROM FLIGHTS F "
    "LEFT JOIN CREW C ON F.FID = C.FlightID "
    "WHERE F.DATEOFFLIGHT = '%s' AND (F.ORIGIN = '%s' OR F.DESTINATION = '%s') "
    "GROUP BY F.FID, F.FNAME, F.ORIGIN, F.DESTINATION, F.DEPARTTIME, F.ARRIVALTIME, F.PRIORITY, F.RUNWAY",
    date, Place, Place);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error executing query: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
        return;
    }
    const char *soundFile = "announcement-sound-104411.wav";
    PlaySound(TEXT(soundFile), NULL, SND_FILENAME | SND_ASYNC);
    printf("Displaying Schedule for Date: %s\n", date);
    printf("---------------------------------------------------------------------------------------------------------------\n");
    printf("| FlightID | Name       | ORIGIN  | DESTINATION | Departure | Arrival  | PRIORITY | Runway | Crew Members        |\n");
    printf("---------------------------------------------------------------------------------------------------------------\n");

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res)) != NULL) {
        printf("| %-8s | %-10s | %-7s | %-11s | %-9s | %-8s | %-8s | %-6s | %-20s |\n", 
            row[0], row[1], row[2], row[3], row[4], row[5], row[6], row[7], row[8] ? row[8] : "None");
    }
    printf("---------------------------------------------------------------------------------------------------------------\n");
    
    mysql_free_result(res);
    Sleep(3000);
}

void assignCrew(char airportID[],char date[],int time) {
    MYSQL_RES *flightRes, *crewRes;
    MYSQL_ROW flightRow, crewRow;

    char flightQuery[256];
    sprintf(flightQuery, "SELECT FID FROM FLIGHTS WHERE TRIM(ORIGIN) = '%s' AND DATEOFFLIGHT = CURDATE()", airportID);

    if (mysql_query(conn, flightQuery)) {
        fprintf(stderr, "Error executing flight query: %s\n", mysql_error(conn));
        return;
    }

    flightRes = mysql_store_result(conn);
    if (flightRes == NULL || mysql_num_rows(flightRes) == 0) {
        mysql_free_result(flightRes);
        return;
    }

    
    while ((flightRow = mysql_fetch_row(flightRes)) != NULL) {
        char flightID[10];
        strcpy(flightID, flightRow[0]); 
        printf("Processing flight %s departing from %s...\n", flightID, airportID);

        
        char crewQuery[256];
        sprintf(crewQuery, "SELECT ID, NAME, LASTFLIGHTTIME FROM CREW WHERE DATEOFFLIGHT = CURDATE()");

        if (mysql_query(conn, crewQuery)) {
            fprintf(stderr, "Error executing crew query: %s\n", mysql_error(conn));
            continue; 
        }

        crewRes = mysql_store_result(conn);
        if (crewRes == NULL) {
            fprintf(stderr, "Error storing crew result: %s\n", mysql_error(conn));
            continue; 
        }

        char assignedCrewIDs[10][10];
        char assignedCrewNames[10][50];
        int assignedCrewCount = 0;

        while ((crewRow = mysql_fetch_row(crewRes)) != NULL && assignedCrewCount < 2) {
            char ID[10], crewNAME[50], LASTFLIGHTTIME[10];
            strcpy(ID, crewRow[0]);
            strcpy(crewNAME, crewRow[1]);
            strcpy(LASTFLIGHTTIME, crewRow[2]);

            char checkQuery[256];
            sprintf(checkQuery, "SELECT COUNT(*) FROM CREW WHERE ID = '%s' AND DATEOFFLIGHT = CURDATE()", ID);

            if (mysql_query(conn, checkQuery)) {
                fprintf(stderr, "Error checking crew assignment for ID %s: %s\n", ID, mysql_error(conn));
                continue;
            }

            MYSQL_RES *checkRes = mysql_store_result(conn);
            MYSQL_ROW checkRow = mysql_fetch_row(checkRes);

            int alreadyAssigned = atoi(checkRow[0]); // Convert count to integer
            mysql_free_result(checkRes); 

            if (alreadyAssigned > 0) {
                
                continue;
            }

            
            strcpy(assignedCrewIDs[assignedCrewCount], ID);
            strcpy(assignedCrewNames[assignedCrewCount], crewNAME);
            assignedCrewCount++;

            
            char updateQuery[256];
            sprintf(updateQuery,
                    "INSERT INTO CREW (ID, NAME, FlightID, LASTFLIGHTTIME, DATEOFFLIGHT) VALUES ('%s', '%s', '%s', CURTIME(), CURDATE())",
                    ID, crewNAME, flightID);

            if (mysql_query(conn, updateQuery)) {
                fprintf(stderr, "Error inserting crew info for crew ID %s: %s\n", ID, mysql_error(conn));
            } else {
                printf("Crew %s assigned to flight %s.\n", crewNAME, flightID);
            }
        }

        mysql_free_result(crewRes); 

        if (assignedCrewCount < 2) {
            fprintf(stderr, "Warning: Unable to assign sufficient crew to flight %s. Manual intervention required.\n", flightID);
        } else {
            printf("Successfully assigned %d crew members to flight %s.\n", assignedCrewCount, flightID);
        }
    }

    mysql_free_result(flightRes); 
}

void delayFlight(int *hour, int *minute, char runwayID) {
    *minute += 30;
    if (*minute >= 60) {
        *minute -= 60;
        (*hour)++;
    }

    if (*hour >= 24) {
        fprintf(stderr, "Warning: Flight delay exceeded daily limits. Cannot delay further.\n");
        return; 
    }
    if (runwayID == 'A') {
        Delayed[0]++;
    } else if (runwayID == 'B') {
        Delayed[1]++;
    } else if (runwayID == 'C') {
        Delayed[2]++;
    }
    playAlertSound();
}
void conflictResolution(char flightID[], int departureHour, int departureMinute, 
    int arrivalHour, int arrivalMinute, int priority, char date[]) {

    MYSQL_RES *res;
    MYSQL_ROW row;

    if (priority == 1) {
        char query[512];
        sprintf(query, 
        "SELECT FID, DEPARTTIME, ARRIVALTIME, RUNWAY FROM FLIGHTS "
        "WHERE RUNWAY IN (SELECT RUNWAY FROM FLIGHTS WHERE FID = '%s'AND DATEOFFLIGHT='%s') "
        "AND FID != '%s' "
        "AND DATEOFFLIGHT='%s'"
        "AND ((DEPARTTIME <= '%02d:%02d' AND ARRIVALTIME > '%02d:%02d') OR "
        "(DEPARTTIME <= '%02d:%02d' AND ARRIVALTIME > '%02d:%02d')) "
        "LIMIT 1",
        flightID,date,flightID, date,departureHour, departureMinute, departureHour, departureMinute,
        arrivalHour, arrivalMinute);

        if (mysql_query(conn, query)) {
            fprintf(stderr, "Error executing query to find clashing flight: %s\n", mysql_error(conn));
            return;
        }

        res = mysql_store_result(conn);
        if (res == NULL) {
            fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
            return;
        }

        row = mysql_fetch_row(res);
        if (row) {
            char clashingFlightID[10];
            int clashingDepartureHour, clashingDepartureMinute;
            sscanf(row[1], "%d:%d", &clashingDepartureHour, &clashingDepartureMinute);

            delayFlight(&clashingDepartureHour, &clashingDepartureMinute, row[3][0]);
            char updateQuery[256];
            sprintf(updateQuery, 
            "UPDATE FLIGHTS SET DEPARTTIME = '%02d:%02d' WHERE FID = '%s' AND DATEOFFLIGHT='%s'",
            clashingDepartureHour, clashingDepartureMinute, row[0],date);

            if (mysql_query(conn, updateQuery)) {
                fprintf(stderr, "Error updating clashing flight: %s\n", mysql_error(conn));
            } else {
                printf("Flight %s delayed to %02d:%02d.\n", row[0], clashingDepartureHour, clashingDepartureMinute);
            }
        } else {
            printf("No clashing flight found for priority 1 flight %s.\n", flightID);
        }

        mysql_free_result(res);
        return; 
    }

    char query[512];
    sprintf(query, 
        "SELECT FID, DEPARTTIME, ARRIVALTIME, RUNWAY FROM FLIGHTS "
        "WHERE RUNWAY IN (SELECT RUNWAY FROM FLIGHTS WHERE FID = '%s'AND DATEOFFLIGHT='%S') "
        "AND FID != '%s' "
        "AND DATEOFFLIGHT='%s'"
        "AND ((DEPARTTIME <= '%02d:%02d' AND ARRIVALTIME > '%02d:%02d') OR "
        "(DEPARTTIME <= '%02d:%02d' AND ARRIVALTIME > '%02d:%02d')) "
        "LIMIT 1",
        flightID,date,flightID, date,departureHour, departureMinute, departureHour, departureMinute,
        arrivalHour, arrivalMinute);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error executing query to find conflicting flight: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
        return;
    }

    row = mysql_fetch_row(res);
    if (row) {
        delayFlight(&departureHour, &departureMinute, row[3][0]);
        delayFlight(&arrivalHour, &arrivalMinute, row[3][0]);

        char updateQuery[256];
        sprintf(updateQuery, 
        "UPDATE FLIGHTS SET DEPARTTIME = '%02d:%02d', ARRIVALTIME = '%02d:%02d' WHERE FID = '%s' AND DATEOFFLIGHT='%s'",
        departureHour, departureMinute, arrivalHour, arrivalMinute, flightID,date);

        if (mysql_query(conn, updateQuery)) {
            fprintf(stderr, "Error updating delayed flight: %s\n", mysql_error(conn));
        } else {
            printf("Flight %s delayed to departure: %02d:%02d, arrival: %02d:%02d.\n", flightID, departureHour, departureMinute, arrivalHour, arrivalMinute);
        }

        char runwayID = assignRunway(departureHour, departureMinute);
        if (runwayID != 'X') {
            sprintf(updateQuery, 
            "UPDATE FLIGHTS SET RUNWAY = '%c' WHERE FID = '%s' AND DATEOFFLIGHT='%s'", runwayID, flightID,date);

            if (mysql_query(conn, updateQuery)) {
                fprintf(stderr, "Error updating runway assignment for flight %s: %s\n", flightID, mysql_error(conn));
            }
        }

        int conflictCount = isRunwayOccupied(runwayID, minutes(departureHour, departureMinute),Place);
        if (conflictCount > 0) {
            printf("Conflict detected even after resolution for flight %s on runway %c.\n", flightID, runwayID);
            printf("Consider manual intervention or further delay.\n");
        }
    }

    mysql_free_result(res);
}

void emergency() {
    int typeOfEmergency;
    printf("Enter type of emergency (0 - Weather, 1 - Flight Cancellation): ");
    scanf("%d", &typeOfEmergency);

    if (typeOfEmergency == 1) { 
        char flightID[10], date[15];
        printf("Enter Flight ID to cancel: ");
        scanf("%s", flightID);
        printf("Enter Date of Flight (YYYY-MM-DD): ");
        scanf("%s", date);

        char deleteFlightQuery[256];
        sprintf(deleteFlightQuery, "DELETE FROM FLIGHTS WHERE FID = '%s' AND DATEOFFLIGHT = '%s'", flightID, date);
        playAlertSound();
        if (mysql_query(conn, deleteFlightQuery)) {
            fprintf(stderr, "Error deleting flight record: %s\n", mysql_error(conn));
        } else {
            printf("Flight %s on %s has been cancelled.\n", flightID, date);
        }

        char deleteCrewQuery[256];
        sprintf(deleteCrewQuery, "DELETE FROM CREW WHERE FlightID = '%s'", flightID);
        if (mysql_query(conn, deleteCrewQuery)) {
            fprintf(stderr, "Error deleting crew records: %s\n", mysql_error(conn));
        } 
        reassignCrewAfterCancellations(date);

    } else if (typeOfEmergency == 0) { 
        char startTime[6], endTime[6];
        int weatherEffect;
        printf("Enter Start Time of bad weather (HH:MM): ");
        scanf("%s", startTime);
        printf("Enter End Time of bad weather (HH:MM): ");
        scanf("%s", endTime);
        printf("Enter weather effect (0 - Mild, 1 - Severe): ");
        scanf("%d", &weatherEffect);

        if (weatherEffect == 0) { 
            printf("Mild weather: Delaying flights...\n");
            playAlertSound();
            char query[256];
            sprintf(query, 
                "SELECT FID, DEPARTTIME, ARRIVALTIME, RUNWAY FROM FLIGHTS "
                "WHERE (DEPARTTIME >= '%s' AND DEPARTTIME <= '%s' AND ORIGIN = '%s') OR "
                "(ARRIVALTIME >= '%s' AND ARRIVALTIME <= '%s' AND DESTINATION = '%s')",
                startTime, endTime, Place, startTime, endTime, Place);

            MYSQL_RES *res;
            MYSQL_ROW row;

            if (mysql_query(conn, query)) {
                fprintf(stderr, "Error fetching affected flights: %s\n", mysql_error(conn));
                return;
            }

            res = mysql_store_result(conn);
            if (res == NULL) {
                fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
                return;
            }
            while ((row = mysql_fetch_row(res)) != NULL) {
                char flightID[10];
                int departureHour, departureMinute, arrivalHour, arrivalMinute;

                strcpy(flightID, row[0]);
                sscanf(row[1], "%d:%d", &departureHour, &departureMinute);
                sscanf(row[2], "%d:%d", &arrivalHour, &arrivalMinute);

                delayFlight(&departureHour, &departureMinute, row[3][0]);
                delayFlight(&arrivalHour, &arrivalMinute, row[3][0]);

                char updateQuery[256];
                sprintf(updateQuery, 
                    "UPDATE FLIGHTS SET DEPARTTIME = '%02d:%02d', ARRIVALTIME = '%02d:%02d' WHERE FID = '%s'",
                    departureHour, departureMinute, arrivalHour, arrivalMinute, flightID);

                if (mysql_query(conn, updateQuery)) {
                    fprintf(stderr, "Error updating delayed flight: %s\n", mysql_error(conn));
                } else {
                    printf("Flight %s delayed to departure: %02d:%02d, arrival: %02d:%02d.\n", flightID, departureHour, departureMinute, arrivalHour, arrivalMinute);
                }
            }
            mysql_free_result(res);

            printf("Updated schedule after delays:\n");
            displaySchedule(date,Place);

        } else if (weatherEffect == 1) { 
            printf("Severe weather: Cancelling flights...\n");
            playAlertSound();
            char deleteFlightQuery[256];
            sprintf(deleteFlightQuery, 
                "DELETE FROM FLIGHTS WHERE "
                "(DEPARTTIME >= '%s' AND DEPARTTIME <= '%s' AND ORIGIN = '%s') OR "
                "(ARRIVALTIME >= '%s' AND ARRIVALTIME <= '%s' AND DESTINATION = '%s')",
                startTime, endTime, Place, startTime, endTime, Place);

            if (mysql_query(conn, deleteFlightQuery)) {
                fprintf(stderr, "Error deleting flights due to severe weather: %s\n", mysql_error(conn));
            } else {
                printf("All flights affected by severe weather have been cancelled.\n");
            }

            char deleteCrewQuery[256];
            sprintf(deleteCrewQuery, 
                "DELETE FROM CREW WHERE "
                "FlightID IN (SELECT FID FROM FLIGHTS WHERE "
                "(DEPARTTIME >= '%s' AND DEPARTTIME <= '%s' AND ORIGIN = '%s') OR "
                "(ARRIVALTIME >= '%s' AND ARRIVALTIME <= '%s' AND DESTINATION = '%s'))",
                startTime, endTime, Place, startTime, endTime, Place);

            if (mysql_query(conn, deleteCrewQuery)) {
                fprintf(stderr, "Error deleting crew records due to severe weather: %s\n", mysql_error(conn));
            } else {
                printf("All crew records for flights affected by severe weather have been removed.\n");
            }

            reassignCrewAfterCancellations(date);
        }
    } else {
        printf("Invalid emergency type entered.\n");
    }
}
void generateRunwayReport(char date[], char airport[]) {
    int takeoffs[3] = {0, 0, 0}; 
    int landings[3] = {0, 0, 0}; 

    char query[512];
    sprintf(query, 
        "SELECT RUNWAY, ORIGIN, DESTINATION, DEPARTTIME, ARRIVALTIME FROM FLIGHTS WHERE DATEOFFLIGHT = '%s'", 
        date);

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error executing query to fetch runway utilization: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_RES *res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
        return;
    }

    MYSQL_ROW row;

   
    while ((row = mysql_fetch_row(res)) != NULL) {
        char runwayID = row[0][0]; 
        char *origin = row[1];    
        char *destination = row[2]; 
        char *departureTime = row[3]; 
        char *arrivalTime = row[4];   
       
        if (strcmp(origin, airport) == 0 && departureTime[0] != '\0') { 
            if (runwayID == 'A') takeoffs[0]++;
            if (runwayID == 'B') takeoffs[1]++;
            if (runwayID == 'C') takeoffs[2]++;
        }

        if (strcmp(destination, airport) == 0 && arrivalTime[0] != '\0') { 
            if (runwayID == 'A') landings[0]++;
            if (runwayID == 'B') landings[1]++;
            if (runwayID == 'C') landings[2]++;
        }
    }

    
    mysql_free_result(res);

    printf("Runway Utilization Report for Airport: %s on Date: %s\n", airport, date);
    printf("--------------------------------------------------------------------\n");
    printf("| Runway | Take-offs | Landings |\n");
    printf("--------------------------------------------------------------------\n");
    printf("|   A    |      %d    |      %d   |\n", takeoffs[0], landings[0]);
    printf("|   B    |      %d    |      %d   |\n", takeoffs[1], landings[1]);
    printf("|   C    |      %d    |      %d   |\n", takeoffs[2], landings[2]);
    printf("--------------------------------------------------------------------\n");
}
void reassignCrewAfterCancellations(char date[]) {
    char flightQuery[256];
    sprintf(flightQuery, 
            "SELECT FID, DEPARTTIME, ARRIVALTIME FROM FLIGHTS "
            "WHERE DATEOFFLIGHT = '%s' AND FID NOT IN (SELECT DISTINCT FlightID FROM CREW)", 
            date);

    MYSQL_RES *flightRes;
    MYSQL_ROW flightRow;

    if (mysql_query(conn, flightQuery)) {
        fprintf(stderr, "Error fetching flights with no crew: %s\n", mysql_error(conn));
        return;
    }

    flightRes = mysql_store_result(conn);
    if (flightRes == NULL) {
        fprintf(stderr, "Error storing flight result: %s\n", mysql_error(conn));
        return;
    }

    if (mysql_num_rows(flightRes) == 0) {
        mysql_free_result(flightRes);
        return;
    }

    while ((flightRow = mysql_fetch_row(flightRes)) != NULL) {
        char flightID[10];
        int departureHour, departureMinute, arrivalHour, arrivalMinute;

        strcpy(flightID, flightRow[0]);

        if (sscanf(flightRow[1], "%d:%d", &departureHour, &departureMinute) != 2 || 
            sscanf(flightRow[2], "%d:%d", &arrivalHour, &arrivalMinute) != 2) {
            fprintf(stderr, "Error parsing time fields for flight %s. \n", flightID);
            continue;
        }

        assignCrew(flightID, date, minutes(departureHour, departureMinute));
    }

    mysql_free_result(flightRes);
}
void allotCrew() {
    char query[512];
    sprintf(query, 
        "SELECT FID, DATEOFFLIGHT, DEPARTTIME "
        "FROM FLIGHTS "
        "WHERE FID NOT IN ("
        "    SELECT DISTINCT FlightID FROM CREW WHERE FlightID IS NOT NULL"
        ") "
        "OR FID IN ("
        "    SELECT FlightID "
        "    FROM CREW "
        "    GROUP BY FlightID "
        "    HAVING COUNT(FlightID) < 2"
        ")");

    MYSQL_RES *res;
    MYSQL_ROW row;

    if (mysql_query(conn, query)) {
        fprintf(stderr, "Error fetching flights with insufficient crew: %s\n", mysql_error(conn));
        return;
    }

    res = mysql_store_result(conn);
    if (res == NULL) {
        fprintf(stderr, "Error storing result: %s\n", mysql_error(conn));
        return;
    }

    if (mysql_num_rows(res) == 0) {
        printf("No flights found that require crew allocation.\n");
        mysql_free_result(res);
        return;
    }

    int flightsProcessed = 0;
    while ((row = mysql_fetch_row(res)) != NULL) {
        char flightID[10];
        char date[15];
        int departureHour = 0, departureMinute = 0;

        strcpy(flightID, row[0]);
        strcpy(date, row[1]);

        if (sscanf(row[2], "%d:%d", &departureHour, &departureMinute) != 2) {
            fprintf(stderr, "Error parsing DEPARTTIME for flight %s. Skipping...\n", flightID);
            continue;
        }

        int departureTimeInMinutes = minutes(departureHour, departureMinute);

        printf("Processing flight %s on %s at %02d:%02d...\n", flightID, date, departureHour, departureMinute);

        assignCrew(flightID, date, departureTimeInMinutes);
        flightsProcessed++;
    }

    mysql_free_result(res);

    if (flightsProcessed > 0) {
        printf("Crew allocation process completed for %d flight(s).\n", flightsProcessed);
    } else {
        printf("No flights required crew allocation.\n");
    }
}

