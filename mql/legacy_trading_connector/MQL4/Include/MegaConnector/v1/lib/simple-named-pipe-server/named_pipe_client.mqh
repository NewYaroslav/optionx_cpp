//+------------------------------------------------------------------+
//|                                            named_pipe_client.mqh |
//|                                      Copyright 2022, Elektro Yar |
//|          https://github.com/NewYaroslav/simple-named-pipe-server |
//+------------------------------------------------------------------+
#property copyright "Copyright 2022, Elektro Yar"
#property link      "https://github.com/NewYaroslav/simple-named-pipe-server"

enum ENUM_PIPE_ACCESS {
    PIPE_ACCESS_INBOUND = 1,
    PIPE_ACCESS_OUTBOUND = 2,
    PIPE_ACCESS_DUPLEX = 3,
};

enum ENUM_PIPE_MODE {
    PIPE_TYPE_RW_BYTE = 0,
    PIPE_TYPE_READ_MESSAGE = 2,
    PIPE_TYPE_WRITE_MESSAGE = 4,
};

#define PIPE_WAIT 0
#define PIPE_NOWAIT 1
#define PIPE_READMODE_MESSAGE 2

#define ERROR_PIPE_CONNECTED 535
#define ERROR_BROKEN_PIPE 109

#define INVALID_HANDLE_VALUE -1
#define GENERIC_READ  0x80000000
#define GENERIC_WRITE  0x40000000
#define OPEN_EXISTING  3
#define PIPE_UNLIMITED_INSTANCES 255
#define PIPE_BUFFER_SIZE 4096
#define STR_SIZE 255

#define PIPE_HANDLE int
#define PIPE_PVOID int
//+------------------------------------------------------------------+
//| DLL imports                                                      |
//+------------------------------------------------------------------+
#import "kernel32.dll"
int   CreateFileW(string name,int desiredAccess,int SharedMode,int security,int creation,int flags,int templateFile);
ulong CreateNamedPipe(string pipeName,int openMode,int pipeMode,int maxInstances,int outbuffer_size,int inbuffer_size,int defaultTimeOut,int security);
int   WaitNamedPipeW(string lpNamedPipeName,int nTimeOut);
bool  SetNamedPipeHandleState(PIPE_HANDLE fileHandle,int &lpMode, int lpMaxCollectionCount,int lpCollectDataTimeout);
int   WriteFile(PIPE_HANDLE file,uchar &buffer[],uint number_of_bytes_to_write,uint &number_of_bytes_written,PIPE_PVOID overlapped);
int   ReadFile(PIPE_HANDLE file,char &buffer[],uint number_of_bytes_to_read,uint &number_of_bytes_read,PIPE_PVOID overlapped);
bool  PeekNamedPipe(PIPE_HANDLE fileHandle, int buffer, int bytes, int bytesRead, int &numOfBytes, int bytesLeftThisMessage);
int   FlushFileBuffers(PIPE_HANDLE pipeHandle);
int   GetLastError(void);
int   CloseHandle(PIPE_HANDLE fileHandle);
#import
//+------------------------------------------------------------------+

/** \brief Класс клиента именованного канала
 */
class NamedPipeClient {
private:
    PIPE_HANDLE  pipe_handle;      // хэндл канала
    string  pipe_name_error_title;
    string  pipe_name;
    string  pipe_name_prefix;
    int     buffer_size;
    int     pipe_id;
    bool    is_connected;
    bool    is_error;
public:

    /** \brief Конструктор класса
     */
    NamedPipeClient() {
        pipe_name_prefix  = "\\\\.\\pipe\\";
        pipe_name_error_title = "NamedPipeClient error! What: ";
        buffer_size       = PIPE_BUFFER_SIZE;
        pipe_handle       = INVALID_HANDLE_VALUE;
        is_connected      = false;
        is_error          = false;
        pipe_id = 0;
        kernel32::GetLastError();
    }

    /** \brief Конструктор класса
     * \param name      Имя именнованного канала
     * \param user_id   Уникальный номер канала
     */
    NamedPipeClient(const string &name, const int user_id = 0) {
        pipe_name_prefix  = "\\\\.\\pipe\\";
        pipe_name_error_title = "NamedPipeClient error! What: ";
        buffer_size       = PIPE_BUFFER_SIZE;
        pipe_handle       = INVALID_HANDLE_VALUE;
        is_connected      = false;
        is_error          = false;
        pipe_id = 0;
        kernel32::GetLastError();
        open(name, user_id);
    }

    /** \brief Деструктор класса
     */
    ~NamedPipeClient() {
        // if (pipe_handle != INVALID_HANDLE_VALUE) CloseHandle(pipe_handle);
        close();
    }

    // callback-функции
    void on_open   (NamedPipeClient *pointer);
    void on_close  (NamedPipeClient *pointer);
    void on_message(NamedPipeClient *pointer, const string &message);
    void on_error  (NamedPipeClient *pointer, const string &error_message);

    /** \brief Проверка наличия соединения
     * \return Вернет true - если соединение было установлено
     */
    bool connected() {
        return is_connected;
    }

    /** \brief Установить размер буфера
     * \param size Размер буфера
     */
    void set_buffer_size(int size) {
        buffer_size = size;
    }

    /** \brief Открывает канал, открытый ранее
     * \param name      Имя именнованного канала
     * \param user_id   Уникальный номер канала
     * \return Вернет true - если успешно, иначе false
     */
    bool open(const string &name, const int user_id = 0) {
        if(pipe_handle == INVALID_HANDLE_VALUE) {
            pipe_name = name;
            pipe_id = user_id;
            const string full_pipe_name = pipe_name_prefix + pipe_name;
            if(WaitNamedPipeW(full_pipe_name, 5000) == 0) {
                const string err_msg_1 = pipe_name_error_title + "Pipe " + full_pipe_name + " busy.";
                if (!is_error) on_error(GetPointer(this), err_msg_1);
                is_error = true;
                return false;
            }

            pipe_handle = CreateFileW(
                              full_pipe_name,
                              (int)(GENERIC_READ | GENERIC_WRITE),
                              0,
                              NULL,
                              OPEN_EXISTING,
                              0,
                              NULL);
            if(pipe_handle == INVALID_HANDLE_VALUE) {
                const string err_msg_2 = pipe_name_error_title + "Pipe open failed.";
                if (!is_error) on_error(GetPointer(this), err_msg_2);
                is_error = true;
                return false;
            }

            /* устанавливаем режим чтения
            * Клиентская сторона именованного канала начинается в байтовом режиме,
            * даже если серверная часть находится в режиме сообщений.
            * Чтобы избежать проблем с получением данных,
            * установите на стороне клиента также режим сообщений.
            * Чтобы изменить режим канала, клиент канала должен
            * открыть канал только для чтения с доступом
            * GENERIC_READ и FILE_WRITE_ATTRIBUTES
            */
            int mode = PIPE_READMODE_MESSAGE;
            bool success = SetNamedPipeHandleState(
                               pipe_handle,
                               mode,
                               NULL,     // не устанавливать максимальные байты
                               NULL);    // не устанавливайте максимальное время

            if(!success) {
                const string err_msg = pipe_name_error_title + "SetNamedPipeHandleState failed.";
                if (!is_error) on_error(GetPointer(this), err_msg);
                is_error = true;
                CloseHandle(pipe_handle);
                pipe_handle = INVALID_HANDLE_VALUE;
                return false;
            }
            on_open(GetPointer(this));
        }
        is_connected = true;
        is_error = false;
        return true;
    }

    /** \brief Закрывает хэндл канала
     * \return 0 если успешно, иначе ненулевое значение
     */
    int close() {
        if (pipe_handle != INVALID_HANDLE_VALUE) {
            int err = CloseHandle(pipe_handle);
            pipe_handle = INVALID_HANDLE_VALUE;
            if (is_connected) on_close(GetPointer(this));
            is_connected = false;
            return err;
        }
        return 0;
    }

    /** \brief Сбрасывает буфер канала
     */
    void flush() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return;
        FlushFileBuffers(pipe_handle);
    }

    /** \brief Записывает строку формата ANSI в канал
     * \param message Строка, содержащее сообщение
     * \return Вернет true, если запись прошла успешно
     */
    bool write(string message) {
        if (pipe_handle == INVALID_HANDLE_VALUE) return false;
        if (StringLen(message) == 0) return false;
        int bytes_to_write, bytes_written;
        uchar utf8_array[];
        bytes_to_write = StringToCharArray(message, utf8_array, 0, -1, CP_UTF8);
        bytes_to_write = ArraySize(utf8_array);
        bytes_to_write--; // При копировании функция StringToCharArray() также копирует символ '\0', завершающий строку.
        WriteFile(
            pipe_handle,
            utf8_array,
            bytes_to_write,
            bytes_written,
            NULL);
        if(bytes_written != bytes_to_write) {
            close();
            return false;
        }
        return true;
    }

    /** \brief Читает строку формата ANSI из канала
     * \return строка в формате Unicode (string в MQL5)
     */
    string read() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return "";
        string ret;
        uchar utf8_array[];
        ArrayResize(utf8_array, buffer_size);
        int bytes_read;
        ReadFile(
            pipe_handle,
            utf8_array,
            buffer_size,
            bytes_read,
            0);
        if(bytes_read != 0) ret = CharArrayToString(utf8_array, 0, bytes_read, CP_UTF8);
        return ret;
    }

    void on_timer() {
        if (is_connected) {
            if (get_bytes_read() > 0) {
                const string message = read();
                on_message(GetPointer(this), message);
            }
        } else {
            open(pipe_name, pipe_id);
        }
    }

    /** \brief Получить количество байтов для чтения
     * \return Количество байтов для чтения
     */
    int get_bytes_read() {
        if (pipe_handle == INVALID_HANDLE_VALUE) return 0;
        int bytes_to_read = 0;
        PeekNamedPipe(
            pipe_handle,
            NULL,
            0,
            NULL,
            bytes_to_read,
            NULL);
        return bytes_to_read;
    };

    /** \brief Возвращает имя канала
     * \return Строка с именем канала
     */
    string get_pipe_name() {
        return pipe_name;
    };

    /** \brief Возвращает PIPE_HANDLE канала
     * \return PIPE_HANDLE канала
     */
    PIPE_HANDLE get_pipe_handle() {
        return pipe_handle;
    };

    /** \brief Возвращает user_id канала
     * \return user_id канала
     */
    int get_pipe_id() {
        return pipe_id;
    };
};
//+------------------------------------------------------------------+
