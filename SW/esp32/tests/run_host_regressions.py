"""Run actual firmware method bodies with fault-injecting host stubs.

Windows: python tests/run_host_regressions.py --zig <path-to-zig.exe>
No device access. Generated DLL and sources stay under build/host_regressions.
"""
import argparse
import ctypes
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--zig', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / 'build/host_regressions'
out.mkdir(parents=True, exist_ok=True)

def method(path, signature):
    text = (root / path).read_text()
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

code = r'''
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned long long size_t;
typedef unsigned int TickType_t;
typedef int esp_err_t;
typedef void* i2c_master_dev_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 258
#define ESP_ERR_INVALID_SIZE 260
#define ESP_ERR_TIMEOUT 263
#define ESP_LOGD(...)
#define ESP_LOGI(...)
#define ESP_LOGW(...)
#define ESP_LOGE(...)
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define SEEK_END 2
#define EOF -1
extern "C" unsigned long _tls_index = 0;
static int errno;
extern "C" void* memset(void* p, int value, size_t n){
    unsigned char* b=(unsigned char*)p; while(n--)*b++=(unsigned char)value; return p;
}
static unsigned tick, notifications, publications, bad_wait, updates, starts, stops;
static int lock_result, bus_result, bus_calls, unlocks, timeout_ms, unlock_result;
static int failed_updates, failed_reads, failed_stops;
static uint8_t sent[2];
static TickType_t xTaskGetTickCount(){return tick;}
static void vTaskDelay(unsigned n){tick+=n;}
static unsigned ulTaskNotifyTake(int, unsigned timeout){
    ++notifications; if(timeout!=portMAX_DELAY)++bad_wait; return 1;
}
static int xQueueOverwrite(void*, const void*){++publications;return 1;}
static int i2c_master_transmit(void*,const uint8_t* p,size_t n,int timeout){
    ++bus_calls;timeout_ms=timeout;if(n==2){sent[0]=p[0];sent[1]=p[1];}return bus_result;
}
static int i2c_master_transmit_receive(void*,const uint8_t*,size_t,uint8_t*,size_t,int timeout){
    ++bus_calls;timeout_ms=timeout;return bus_result;
}
namespace i2c_base {
class I2C {
public:
    static constexpr int I2C_TIMEOUT=1000;
    esp_err_t take_i2c_port_semaphore(){return lock_result;}
    esp_err_t release_i2c_port_semaphore(){++unlocks;return unlock_result;}
    esp_err_t read_byte(void*,const uint8_t*,size_t,uint8_t*);
    esp_err_t read_data(void*,const uint8_t*,size_t,uint8_t*,size_t);
    esp_err_t write_byte(void*,const uint8_t*,size_t,uint8_t);
};
}
class BME280 {
public:
    struct {float temperature;double humidity;float pressure;} results_data;
    void* results_buffer;
    esp_err_t update_sensor_data(){++updates;return failed_updates-- > 0 ? ESP_FAIL : ESP_OK;}
    void measure_task();
};
class SCD40 {
public:
    static constexpr unsigned COMMAND_INTERVAL=500, MEASUREMENT_INTERVAL=6000;
    uint16_t co2;void* co2_buffer;
    esp_err_t start_periodic_measurement(){++starts;return ESP_OK;}
    esp_err_t stop_periodic_measurement(){++stops;return failed_stops-- > 0 ? ESP_FAIL : ESP_OK;}
    esp_err_t get_co2_data(uint16_t*){return failed_reads-- > 0 ? ESP_FAIL : ESP_OK;}
    void measure_co2_task();
};
class EPAPER4IN26 {
public:
    enum class state_e {NOT_INITIALIZED,INITIALIZED};
    state_e mstate; bool stuck;
    int is_busy(){return stuck;}
    esp_err_t wait_until_ready();
};
struct FILE{};
static FILE file;
static char disk[4096];static long disk_size;
static bool fail_open,fail_write,fail_flush,fail_close;
static int writes,closes;
static int snprintf(char* p,size_t n,const char*,const char* a,const char* b){
    size_t pos=0;while(*a){if(pos+1<n)p[pos]=*a;++pos;++a;}
    while(*b){if(pos+1<n)p[pos]=*b;++pos;++b;}
    if(n)p[pos<n?pos:n-1]=0;return (int)pos;
}
static FILE* fopen(const char*,const char* mode){
    if(fail_open)return nullptr;if(mode[0]=='w')disk_size=0;return &file;
}
static int fseek(FILE*,long,int){return 0;}
static long ftell(FILE*){return disk_size;}
static int fputs(const char* p,FILE*){
    if(fail_write)return EOF;++writes;while(*p)disk[disk_size++]=*p++;return 0;
}
static int fflush(FILE*){return fail_flush?-1:0;}
static int fclose(FILE*){++closes;return fail_close?-1:0;}
class SD_CARD {
public:
    static constexpr char MOUNT_POINT[]="/sdcard";
    esp_err_t write_data(const char*,size_t,char*,char);
};
'''
code += r'''
static int critical_depth, critical_violation, register_result, unregister_result;
#define taskENTER_CRITICAL(x) (++critical_depth)
#define taskEXIT_CRITICAL(x) (--critical_depth)
namespace std {
class mutex {public: void lock(){} void unlock(){}};
template<class T> class lock_guard {T& value;public: lock_guard(T& v):value(v){value.lock();}~lock_guard(){value.unlock();}};
}
typedef void (*esp_event_handler_t)();
typedef void* esp_event_loop_handle_t;
static void* INPUT_EVENTS;
static int esp_event_handler_instance_register(void*,int,esp_event_handler_t,void*,void** instance){
    if(critical_depth)++critical_violation;
    if(!register_result)*instance=(void*)1;return register_result;
}
static int esp_event_handler_instance_register_with(void*,void*,int,esp_event_handler_t,void*,void** instance){
    if(critical_depth)++critical_violation;
    if(!register_result)*instance=(void*)2;return register_result;
}
static int esp_event_handler_instance_unregister(void*,int,void*){
    if(critical_depth)++critical_violation;return unregister_result;
}
static int esp_event_handler_instance_unregister_with(void*,void*,int,void*){
    if(critical_depth)++critical_violation;return unregister_result;
}
namespace GpioInterface {
class GpioInput {
public:
    struct interrupt_args_t {
        bool mevent_handler_set=false,mcustom_event_handler_set=false,mqueue_enable=false;
        int mpin=1;void* mcustom_event_loop_handle=nullptr;void* mqueue_handle=nullptr;
    } minterrupt_args;
    std::mutex mregistration_mutex;bool mevent_handler_set=false;
    esp_event_handler_t mevent_handle=nullptr;void* mevent_instance=nullptr;
    esp_err_t set_event_handler(esp_event_handler_t);
    esp_err_t set_event_handler(esp_event_loop_handle_t,esp_event_handler_t);
    void set_queue_handle(void*);
    esp_err_t mclear_event_handlers();
};
}
static int display_notifications;
static void xTaskNotifyGive(void*){++display_notifications;}
typedef void* TaskHandle_t; typedef void* QueueHandle_t;
class SMART_CLOCK {
public:
    struct Flag{bool value=false;bool load(){return value;}} application_ready;
    struct Handle{void* value=nullptr;void* load(){return value;}} update_display_handle;
    static void notify_display_update(void*);
    void update_display_timer_task();
};
static void handler(){}
'''
for signature in ['esp_err_t GpioInput::set_event_handler(esp_event_handler_t',
                  'esp_err_t GpioInput::set_event_handler(esp_event_loop_handle_t',
                  'void GpioInput::set_queue_handle', 'esp_err_t GpioInput::mclear_event_handlers']:
    code += '\nnamespace GpioInterface {\n' + method('components/gpio/gpio_input.cpp', signature) + '\n}\n'
for signature in ['void SMART_CLOCK::notify_display_update', 'void SMART_CLOCK::update_display_timer_task']:
    code += '\n' + method('main/smart_clock.cpp', signature) + '\n'
for signature in ['esp_err_t I2C::read_byte', 'esp_err_t I2C::read_data', 'esp_err_t I2C::write_byte']:
    code += '\nnamespace i2c_base {\n' + method('components/i2c/i2c_base.cpp', signature) + '\n}\n'
for path, signature in [
    ('components/bme280/bme280.cpp', 'void BME280::measure_task'),
    ('components/scd40/scd40.cpp', 'void SCD40::measure_co2_task'),
    ('components/display/e_paper4in26.cpp', 'esp_err_t EPAPER4IN26::wait_until_ready'),
    ('components/sd_card/sd_card.cpp', 'esp_err_t SD_CARD::write_data'),
]:
    body = method(path, signature)
    if signature.startswith('void'):
        # Three queued measurement notifications; stop after processing them.
        body = body.replace('while(true)', 'for(int request=0;request<3;++request)', 1)
    code += '\n' + body + '\n'
code += r'''
extern "C" int DllMain(void*, unsigned, void*){ return 1; }
extern "C" __declspec(dllexport) int run_tests(){
    i2c_base::I2C i2c;uint8_t command=0xf4,data=0;
    lock_result=ESP_ERR_TIMEOUT;bus_calls=unlocks=0;
    if(i2c.read_byte(nullptr,&command,1,&data)!=ESP_ERR_TIMEOUT || bus_calls || unlocks)return 1;
    if(i2c.read_data(nullptr,&command,1,&data,1)!=ESP_ERR_TIMEOUT || bus_calls || unlocks)return 2;
    if(i2c.write_byte(nullptr,&command,1,0x25)!=ESP_ERR_TIMEOUT || bus_calls || unlocks)return 3;
    lock_result=0;bus_result=ESP_ERR_TIMEOUT;
    if(i2c.read_byte(nullptr,&command,1,&data)!=ESP_ERR_TIMEOUT || timeout_ms!=1000 || unlocks!=1)return 4;
    bus_result=0;
    if(i2c.write_byte(nullptr,&command,1,0x25)!=0 || sent[0]!=0xf4 || sent[1]!=0x25 || timeout_ms!=1000)return 5;
    if(i2c.write_byte(nullptr,&command,2,0)!=ESP_ERR_INVALID_ARG)return 6;
    bus_result=ESP_ERR_TIMEOUT;unlock_result=ESP_FAIL;
    if(i2c.read_data(nullptr,&command,1,&data,1)!=ESP_ERR_TIMEOUT)return 19;
    bus_result=0;
    if(i2c.read_byte(nullptr,&command,1,&data)!=ESP_FAIL)return 20;
    unlock_result=0;
    BME280 bme;notifications=publications=bad_wait=updates=0;failed_updates=2;
    bme.measure_task();
    if(notifications!=3 || bad_wait || publications!=2 || updates!=4)return 7;
    SCD40 scd;notifications=publications=bad_wait=starts=stops=0;failed_reads=1;failed_stops=1;
    scd.measure_co2_task();
    if(notifications!=3 || bad_wait || publications!=2 || starts!=3 || stops!=4)return 8;
    EPAPER4IN26 panel;panel.stuck=true;panel.mstate=EPAPER4IN26::state_e::INITIALIZED;tick=0;
    if(panel.wait_until_ready()!=ESP_ERR_TIMEOUT || tick!=30000 || panel.mstate!=EPAPER4IN26::state_e::NOT_INITIALIZED)return 9;
    panel.stuck=false;if(panel.wait_until_ready()!=0)return 10;
    SD_CARD sd;char header[]="Humidity[%]\n",row[]="2026/10/10, 50%\n";
    disk_size=0;writes=closes=0;
    if(sd.write_data("/sensor.csv",12,header,'h')!=0 || writes!=1)return 11;
    long header_size=disk_size;
    if(sd.write_data("/sensor.csv",12,row,'a')!=0 || disk_size<=header_size)return 12;
    long preserved=disk_size;
    if(sd.write_data("/sensor.csv",12,header,'h')!=0 || disk_size!=preserved || writes!=2)return 13;
    if(disk[8]!='[' || disk[9]!='%' || disk[10]!=']')return 14;
    fail_write=true;int closed=closes;
    if(sd.write_data("/sensor.csv",12,row,'a')!=ESP_FAIL || closes!=closed+1)return 15;
    fail_write=false;fail_flush=true;
    if(sd.write_data("/sensor.csv",12,row,'a')!=ESP_FAIL)return 16;
    fail_flush=false;fail_close=true;
    if(sd.write_data("/sensor.csv",12,row,'a')!=ESP_FAIL)return 17;
    fail_close=false;fail_open=true;
    if(sd.write_data("/sensor.csv",12,row,'a')!=ESP_FAIL)return 18;
    GpioInterface::GpioInput gpio;
    if(gpio.set_event_handler(&handler)!=0 || !gpio.mevent_instance || !gpio.minterrupt_args.mevent_handler_set || critical_depth)return 21;
    unregister_result=ESP_FAIL;
    if(gpio.set_event_handler((void*)3,&handler)!=ESP_FAIL || !gpio.minterrupt_args.mevent_handler_set || !gpio.mevent_instance || critical_depth)return 22;
    unregister_result=0;
    if(gpio.set_event_handler((void*)3,&handler)!=0 || !gpio.minterrupt_args.mcustom_event_handler_set || gpio.minterrupt_args.mevent_handler_set)return 23;
    gpio.set_queue_handle((void*)4);
    if(!gpio.minterrupt_args.mqueue_enable || gpio.minterrupt_args.mcustom_event_handler_set || gpio.mevent_instance || critical_depth || critical_violation)return 24;
    if(gpio.set_event_handler(nullptr)!=ESP_ERR_INVALID_ARG || !gpio.minterrupt_args.mqueue_enable)return 25;
    SMART_CLOCK clock;clock.update_display_handle.value=(void*)1;
    clock.update_display_timer_task();SMART_CLOCK::notify_display_update(&clock);
    if(display_notifications)return 26;
    clock.application_ready.value=true;
    clock.update_display_timer_task();
    if(display_notifications!=1)return 27;
    clock.update_display_handle.value=nullptr;SMART_CLOCK::notify_display_update(&clock);
    if(display_notifications!=1)return 28;
    return 0;
}
'''
source = out / 'regressions.cpp'
source.write_text(code, encoding='utf-8')
obj = out / 'regressions.obj'
dll = out / 'regressions.dll'
subprocess.run([str(args.zig), 'c++', '-target', 'x86_64-windows-gnu', '-shared', '-nostdlib',
                '-Wl,--entry,DllMain', '-O0', '-fno-stack-protector', '-fno-exceptions', '-fno-rtti',
                '-fno-builtin', str(source), '-o', str(dll)], check=True)
result = ctypes.CDLL(str(dll)).run_tests()
if result:
    raise SystemExit(f'FAIL: regression case {result}')
print('PASS: 28 regression checks (I2C, sensors, BUSY, CSV, GPIO lifecycle, startup notifications)')
