#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <winsock2.h>
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#include <iphlpapi.h>

#define SERVER_HOST "127.0.0.1"
#define SERVER_PORT 5000

#define MAX_PROCESSES 1024
#define PROCESS_RANKING_INTERVAL 5
#define MONITOR_INTERVAL_MS 10000

typedef struct
{
    DWORD pid;
    char name[MAX_PATH];
    double memory_mb;
} ProcessInfo;


/* Function prototypes */

double calculate_cpu_usage(
    FILETIME idle1,
    FILETIME kernel1,
    FILETIME user1,
    FILETIME idle2,
    FILETIME kernel2,
    FILETIME user2
);

double get_memory_usage(void);

double get_disk_usage(void);

int get_network_bytes(
    unsigned long long *rx_bytes,
    unsigned long long *tx_bytes
);

void display_process_info(void);

void display_process_ranking(void);

void log_system_info(
    const char *timestamp,
    double cpu_usage,
    double memory_usage,
    double disk_usage,
    double network_rx_kbps,
    double network_tx_kbps
);

int send_all(
    SOCKET socket,
    const char *data,
    int length
);


/*
 * Calculate CPU usage from two system-time snapshots.
 */
double calculate_cpu_usage(
    FILETIME idle1,
    FILETIME kernel1,
    FILETIME user1,
    FILETIME idle2,
    FILETIME kernel2,
    FILETIME user2
)
{
    ULARGE_INTEGER idle_time1;
    ULARGE_INTEGER kernel_time1;
    ULARGE_INTEGER user_time1;

    ULARGE_INTEGER idle_time2;
    ULARGE_INTEGER kernel_time2;
    ULARGE_INTEGER user_time2;

    idle_time1.LowPart = idle1.dwLowDateTime;
    idle_time1.HighPart = idle1.dwHighDateTime;

    kernel_time1.LowPart = kernel1.dwLowDateTime;
    kernel_time1.HighPart = kernel1.dwHighDateTime;

    user_time1.LowPart = user1.dwLowDateTime;
    user_time1.HighPart = user1.dwHighDateTime;

    idle_time2.LowPart = idle2.dwLowDateTime;
    idle_time2.HighPart = idle2.dwHighDateTime;

    kernel_time2.LowPart = kernel2.dwLowDateTime;
    kernel_time2.HighPart = kernel2.dwHighDateTime;

    user_time2.LowPart = user2.dwLowDateTime;
    user_time2.HighPart = user2.dwHighDateTime;

    unsigned long long idle =
        idle_time2.QuadPart - idle_time1.QuadPart;

    unsigned long long kernel =
        kernel_time2.QuadPart - kernel_time1.QuadPart;

    unsigned long long user =
        user_time2.QuadPart - user_time1.QuadPart;

    unsigned long long total =
        kernel + user;

    if (total == 0)
    {
        return 0.0;
    }

    double usage =
        (double)(total - idle) /
        (double)total *
        100.0;

    if (usage < 0.0)
    {
        usage = 0.0;
    }

    if (usage > 100.0)
    {
        usage = 100.0;
    }

    return usage;
}


/*
 * Get physical memory usage percentage.
 */
double get_memory_usage(void)
{
    MEMORYSTATUSEX memory_status;

    memset(
        &memory_status,
        0,
        sizeof(memory_status)
    );

    memory_status.dwLength =
        sizeof(memory_status);

    if (GlobalMemoryStatusEx(&memory_status) == 0)
    {
        fprintf(
            stderr,
            "ERROR: Failed to get memory information.\n"
        );

        return -1.0;
    }

    if (memory_status.ullTotalPhys == 0)
    {
        return -1.0;
    }

    double usage =
        (
            (double)(
                memory_status.ullTotalPhys -
                memory_status.ullAvailPhys
            )
            /
            (double)memory_status.ullTotalPhys
        )
        * 100.0;

    return usage;
}


/*
 * Get C: drive usage percentage.
 */
double get_disk_usage(void)
{
    ULARGE_INTEGER free_bytes_available;
    ULARGE_INTEGER total_bytes;
    ULARGE_INTEGER free_bytes;

    if (GetDiskFreeSpaceExA(
            "C:\\",
            &free_bytes_available,
            &total_bytes,
            &free_bytes) == 0)
    {
        fprintf(
            stderr,
            "ERROR: Failed to get disk information.\n"
        );

        return -1.0;
    }

    if (total_bytes.QuadPart == 0)
    {
        return -1.0;
    }

    double usage =
        (
            (double)(
                total_bytes.QuadPart -
                free_bytes.QuadPart
            )
            /
            (double)total_bytes.QuadPart
        )
        * 100.0;

    return usage;
}


/*
 * Get total network RX/TX bytes from active interfaces.
 */
int get_network_bytes(
    unsigned long long *rx_bytes,
    unsigned long long *tx_bytes
)
{
    DWORD buffer_size = 0;

    DWORD result =
        GetIfTable(
            NULL,
            &buffer_size,
            FALSE
        );

    if (
        result != ERROR_INSUFFICIENT_BUFFER &&
        result != NO_ERROR
    )
    {
        return 0;
    }

    PMIB_IFTABLE table =
        (PMIB_IFTABLE)malloc(buffer_size);

    if (table == NULL)
    {
        return 0;
    }

    result =
        GetIfTable(
            table,
            &buffer_size,
            FALSE
        );

    if (result != NO_ERROR)
    {
        free(table);
        return 0;
    }

    unsigned long long total_rx = 0;
    unsigned long long total_tx = 0;

    for (DWORD i = 0; i < table->dwNumEntries; i++)
    {
        MIB_IFROW *row =
            &table->table[i];

        if (
            row->dwOperStatus == MIB_IF_OPER_STATUS_OPERATIONAL
        )
        {
            total_rx += row->dwInOctets;
            total_tx += row->dwOutOctets;
        }
    }

    free(table);

    *rx_bytes = total_rx;
    *tx_bytes = total_tx;

    return 1;
}


/*
 * Send the complete buffer even when send()
 * transmits only part of it.
 */
int send_all(
    SOCKET socket,
    const char *data,
    int length
)
{
    int total_sent = 0;

    while (total_sent < length)
    {
        int sent =
            send(
                socket,
                data + total_sent,
                length - total_sent,
                0
            );

        if (sent == SOCKET_ERROR)
        {
            return 0;
        }

        if (sent == 0)
        {
            return 0;
        }

        total_sent += sent;
    }

    return 1;
}


/*
 * Display all accessible processes.
 */
void display_process_info(void)
{
    HANDLE snapshot;

    PROCESSENTRY32 process_entry;

    snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        fprintf(
            stderr,
            "ERROR: Failed to create process snapshot.\n"
        );

        return;
    }

    memset(
        &process_entry,
        0,
        sizeof(process_entry)
    );

    process_entry.dwSize =
        sizeof(PROCESSENTRY32);

    printf("\nProcess Information\n");
    printf("------------------------------------------------\n");
    printf(
        "%-8s %-30s %12s\n",
        "PID",
        "Process Name",
        "Memory"
    );
    printf("------------------------------------------------\n");

    if (Process32First(
            snapshot,
            &process_entry))
    {
        do
        {
            HANDLE process_handle =
                OpenProcess(
                    PROCESS_QUERY_INFORMATION |
                    PROCESS_VM_READ,
                    FALSE,
                    process_entry.th32ProcessID
                );

            if (process_handle != NULL)
            {
                PROCESS_MEMORY_COUNTERS memory_counters;

                if (GetProcessMemoryInfo(
                        process_handle,
                        &memory_counters,
                        sizeof(memory_counters)))
                {
                    double memory_mb =
                        (double)memory_counters.WorkingSetSize /
                        (1024.0 * 1024.0);

                    printf(
                        "%-8lu %-30s %9.2f MB\n",
                        process_entry.th32ProcessID,
                        process_entry.szExeFile,
                        memory_mb
                    );
                }

                CloseHandle(process_handle);
            }

        }
        while (
            Process32Next(
                snapshot,
                &process_entry
            )
        );
    }

    CloseHandle(snapshot);
}


/*
 * Collect processes, sort by memory usage,
 * display TOP10, and append the ranking to CSV.
 */
void display_process_ranking(void)
{
    HANDLE snapshot;

    PROCESSENTRY32 process_entry;

    ProcessInfo processes[MAX_PROCESSES];

    int count = 0;
    int skipped_processes = 0;

    snapshot =
        CreateToolhelp32Snapshot(
            TH32CS_SNAPPROCESS,
            0
        );

    if (snapshot == INVALID_HANDLE_VALUE)
    {
        fprintf(
            stderr,
            "ERROR: Failed to create process snapshot.\n"
        );

        return;
    }

    memset(
        &process_entry,
        0,
        sizeof(process_entry)
    );

    process_entry.dwSize =
        sizeof(PROCESSENTRY32);

    if (!Process32First(
            snapshot,
            &process_entry))
    {
        fprintf(
            stderr,
            "ERROR: Failed to get first process information.\n"
        );

        CloseHandle(snapshot);

        return;
    }

    do
    {
        if (count >= MAX_PROCESSES)
        {
            break;
        }

        HANDLE process_handle =
            OpenProcess(
                PROCESS_QUERY_INFORMATION |
                PROCESS_VM_READ,
                FALSE,
                process_entry.th32ProcessID
            );

        if (process_handle != NULL)
        {
            PROCESS_MEMORY_COUNTERS memory_counters;

            if (GetProcessMemoryInfo(
                    process_handle,
                    &memory_counters,
                    sizeof(memory_counters)))
            {
                processes[count].pid =
                    process_entry.th32ProcessID;

                snprintf(
                    processes[count].name,
                    MAX_PATH,
                    "%s",
                    process_entry.szExeFile
                );

                processes[count].memory_mb =
                    (double)memory_counters.WorkingSetSize /
                    (1024.0 * 1024.0);

                count++;
            }
            else
            {
                skipped_processes++;
            }

            CloseHandle(process_handle);
        }
        else
        {
            skipped_processes++;
        }

    }
    while (
        Process32Next(
            snapshot,
            &process_entry
        )
    );

    CloseHandle(snapshot);


    /* Sort processes by memory usage descending. */
    for (int i = 0; i < count - 1; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (
                processes[j].memory_mb >
                processes[i].memory_mb
            )
            {
                ProcessInfo temp =
                    processes[i];

                processes[i] =
                    processes[j];

                processes[j] =
                    temp;
            }
        }
    }


    int limit =
        count < 10
        ? count
        : 10;

    printf("\nProcess Memory Ranking\n");
    printf("------------------------------------------------\n");
    printf(
        "%-6s %-8s %-30s %12s\n",
        "Rank",
        "PID",
        "Process Name",
        "Memory"
    );
    printf("------------------------------------------------\n");

    for (int i = 0; i < limit; i++)
    {
        printf(
            "%-6d %-8lu %-30s %9.2f MB\n",
            i + 1,
            processes[i].pid,
            processes[i].name,
            processes[i].memory_mb
        );
    }


    FILE *file =
        fopen(
            "logs/process_ranking.csv",
            "a"
        );

    if (file == NULL)
    {
        fprintf(
            stderr,
            "ERROR: Failed to open process ranking log.\n"
        );

        return;
    }


    SYSTEMTIME st;

    GetLocalTime(&st);

    char timestamp[32];

    snprintf(
        timestamp,
        sizeof(timestamp),
        "%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond
    );


    fseek(
        file,
        0,
        SEEK_END
    );

    long file_size =
        ftell(file);

    if (file_size == 0)
    {
        fprintf(
            file,
            "timestamp,rank,pid,process_name,memory_mb\n"
        );
    }


    for (int i = 0; i < limit; i++)
    {
        fprintf(
            file,
            "%s,%d,%lu,%s,%.2f\n",
            timestamp,
            i + 1,
            processes[i].pid,
            processes[i].name,
            processes[i].memory_mb
        );
    }

    fclose(file);

    printf(
        "\nProcess ranking logged successfully."
        " Skipped: %d\n",
        skipped_processes
    );
}


/*
 * Append system metrics to CSV.
 */
void log_system_info(
    const char *timestamp,
    double cpu_usage,
    double memory_usage,
    double disk_usage,
    double network_rx_kbps,
    double network_tx_kbps
)
{
    FILE *file =
        fopen(
            "logs/system_monitor.csv",
            "a"
        );

    if (file == NULL)
    {
        fprintf(
            stderr,
            "ERROR: Failed to open system monitor log file.\n"
        );

        return;
    }


    fseek(
        file,
        0,
        SEEK_END
    );

    long file_size =
        ftell(file);

    if (file_size == 0)
    {
        fprintf(
            file,
            "timestamp,cpu_usage,memory_usage,disk_usage,"
            "network_rx_kbps,network_tx_kbps\n"
        );
    }


    fprintf(
        file,
        "%s,%.2f,%.2f,%.2f,%.2f,%.2f\n",
        timestamp,
        cpu_usage,
        memory_usage,
        disk_usage,
        network_rx_kbps,
        network_tx_kbps
    );

    fclose(file);
}


int main(void)
{
    WSADATA wsa_data;

    if (
        WSAStartup(
            MAKEWORD(2, 2),
            &wsa_data
        ) != 0
    )
    {
        fprintf(
            stderr,
            "ERROR: Failed to initialize Winsock.\n"
        );

        return 1;
    }


    SOCKET server_socket =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    if (server_socket == INVALID_SOCKET)
    {
        fprintf(
            stderr,
            "ERROR: Failed to create socket.\n"
        );

        WSACleanup();

        return 1;
    }


    struct sockaddr_in server_address;

    memset(
        &server_address,
        0,
        sizeof(server_address)
    );

    server_address.sin_family =
        AF_INET;

    server_address.sin_addr.s_addr =
        inet_addr(SERVER_HOST);

    server_address.sin_port =
        htons(SERVER_PORT);


    if (
        bind(
            server_socket,
            (struct sockaddr *)&server_address,
            sizeof(server_address)
        ) == SOCKET_ERROR
    )
    {
        fprintf(
            stderr,
            "ERROR: Failed to bind socket. Error: %d\n",
            WSAGetLastError()
        );

        closesocket(server_socket);
        WSACleanup();

        return 1;
    }


    if (
        listen(
            server_socket,
            5
        ) == SOCKET_ERROR
    )
    {
        fprintf(
            stderr,
            "ERROR: Failed to listen on socket.\n"
        );

        closesocket(server_socket);
        WSACleanup();

        return 1;
    }


    printf("========================================\n");
    printf("       C SYSTEM MONITOR Ver.4\n");
    printf("========================================\n\n");

    printf(
        "TCP Server : %s:%d\n",
        SERVER_HOST,
        SERVER_PORT
    );

    printf(
        "Status     : Listening\n\n"
    );


    SYSTEM_INFO system_info;

    GetSystemInfo(
        &system_info
    );

    printf(
        "CPU Cores  : %lu\n",
        system_info.dwNumberOfProcessors
    );

    printf(
        "\nWaiting for Python Dashboard...\n"
    );


    int measurement_count = 1;


    while (1)
    {
        struct sockaddr_in client_address;

        int client_address_size =
            sizeof(client_address);

        SOCKET client_socket =
            accept(
                server_socket,
                (struct sockaddr *)&client_address,
                &client_address_size
            );

        if (client_socket == INVALID_SOCKET)
        {
            fprintf(
                stderr,
                "ERROR: Failed to accept client.\n"
            );

            continue;
        }


        printf(
            "\nPython Dashboard connected.\n"
        );


        unsigned long long previous_rx = 0;
        unsigned long long previous_tx = 0;

        int has_previous_network =
            get_network_bytes(
                &previous_rx,
                &previous_tx
            );


        while (1)
        {
            printf(
                "\n========== Measurement %d ==========\n",
                measurement_count
            );


            FILETIME idle1;
            FILETIME kernel1;
            FILETIME user1;

            FILETIME idle2;
            FILETIME kernel2;
            FILETIME user2;


            if (
                !GetSystemTimes(
                    &idle1,
                    &kernel1,
                    &user1
                )
            )
            {
                fprintf(
                    stderr,
                    "ERROR: Failed to get initial system times.\n"
                );

                break;
            }


            Sleep(1000);


            if (
                !GetSystemTimes(
                    &idle2,
                    &kernel2,
                    &user2
                )
            )
            {
                fprintf(
                    stderr,
                    "ERROR: Failed to get second system times.\n"
                );

                break;
            }


            double cpu_usage =
                calculate_cpu_usage(
                    idle1,
                    kernel1,
                    user1,
                    idle2,
                    kernel2,
                    user2
                );


            double memory_usage =
                get_memory_usage();

            if (memory_usage < 0.0)
            {
                fprintf(
                    stderr,
                    "ERROR: Failed to retrieve memory usage.\n"
                );

                break;
            }


            double disk_usage =
                get_disk_usage();

            if (disk_usage < 0.0)
            {
                fprintf(
                    stderr,
                    "ERROR: Failed to retrieve disk usage.\n"
                );

                break;
            }


            unsigned long long current_rx = 0;
            unsigned long long current_tx = 0;

            double network_rx_kbps = 0.0;
            double network_tx_kbps = 0.0;


            if (
                get_network_bytes(
                    &current_rx,
                    &current_tx
                )
            )
            {
                if (has_previous_network)
                {
                    if (current_rx >= previous_rx)
                    {
                        network_rx_kbps =
                            (
                                (double)(
                                    current_rx -
                                    previous_rx
                                )
                                / 1024.0
                            );
                    }

                    if (current_tx >= previous_tx)
                    {
                        network_tx_kbps =
                            (
                                (double)(
                                    current_tx -
                                    previous_tx
                                )
                                / 1024.0
                            );
                    }
                }

                previous_rx =
                    current_rx;

                previous_tx =
                    current_tx;

                has_previous_network = 1;
            }


            SYSTEMTIME current_time;

            GetLocalTime(
                &current_time
            );

            char timestamp[32];

            snprintf(
                timestamp,
                sizeof(timestamp),
                "%04d-%02d-%02d %02d:%02d:%02d",
                current_time.wYear,
                current_time.wMonth,
                current_time.wDay,
                current_time.wHour,
                current_time.wMinute,
                current_time.wSecond
            );


            char message[512];

            snprintf(
                message,
                sizeof(message),
                "{\"timestamp\":\"%s\","
                "\"cpu\":%.2f,"
                "\"memory\":%.2f,"
                "\"disk\":%.2f,"
                "\"network_rx_kbps\":%.2f,"
                "\"network_tx_kbps\":%.2f}\n",
                timestamp,
                cpu_usage,
                memory_usage,
                disk_usage,
                network_rx_kbps,
                network_tx_kbps
            );


            if (
                !send_all(
                    client_socket,
                    message,
                    (int)strlen(message)
                )
            )
            {
                fprintf(
                    stderr,
                    "Python Dashboard disconnected.\n"
                );

                closesocket(
                    client_socket
                );

                client_socket =
                    INVALID_SOCKET;

                break;
            }


            printf(
                "CPU        : %.2f %%\n",
                cpu_usage
            );

            printf(
                "Memory     : %.2f %%\n",
                memory_usage
            );

            printf(
                "Disk       : %.2f %%\n",
                disk_usage
            );

            printf(
                "Network RX : %.2f KB/s\n",
                network_rx_kbps
            );

            printf(
                "Network TX : %.2f KB/s\n",
                network_tx_kbps
            );


            log_system_info(
                timestamp,
                cpu_usage,
                memory_usage,
                disk_usage,
                network_rx_kbps,
                network_tx_kbps
            );


            /*
             * Update process ranking periodically
             * instead of every measurement.
             */
            if (
                measurement_count %
                PROCESS_RANKING_INTERVAL == 0
            )
            {
                display_process_ranking();
            }


            printf(
                "\nNext measurement in %d seconds...\n",
                MONITOR_INTERVAL_MS / 1000
            );


            Sleep(
                MONITOR_INTERVAL_MS
            );


            measurement_count++;
        }


        if (client_socket != INVALID_SOCKET)
        {
            closesocket(
                client_socket
            );
        }


        printf(
            "\nWaiting for Python Dashboard to reconnect...\n"
        );
    }


    closesocket(
        server_socket
    );

    WSACleanup();

    return 0;
}
// gcc src/main.c -o build/monitor.exe -lpsapi -lws2_32 -liphlpapi
// .\build\monitor.exe