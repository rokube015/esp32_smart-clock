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
    text = (root / path).read_text(encoding='utf-8')
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
typedef unsigned int uint32_t;
typedef int esp_err_t;
typedef void* i2c_master_dev_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG 258
#define ESP_ERR_INVALID_SIZE 260
#define ESP_ERR_TIMEOUT 263
#define ESP_ERR_NOT_FOUND 261
#define ENOENT 2
#define ESP_LOGD(...)
#define ESP_LOGI(...)
#define ESP_LOGW(...)
#define ESP_LOGE(...)
#define pdTRUE 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define SEEK_END 2
#define SEEK_SET 0
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
    struct results_data_t {float temperature;double humidity;float pressure;};
    results_data_t results_data;
    void notify_measurement_start(){}
    void* results_buffer;
    esp_err_t update_sensor_data(){++updates;return failed_updates-- > 0 ? ESP_FAIL : ESP_OK;}
    void measure_task();
};
class SCD40 {
public:
    static constexpr unsigned COMMAND_INTERVAL=500, MEASUREMENT_INTERVAL=6000;
    uint16_t co2;void* co2_buffer;
    void notify_measurement_start(){}
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
struct FILE{long pos;};
static FILE file;
static char disk[4096];static long disk_size;
static bool fail_open,fail_write,fail_flush,fail_close;
static bool fail_seek,fail_tell,append_on_read;
static int read_calls,fail_read_at=-1;
static int writes,closes;
static int snprintf(char* p,size_t n,const char*,const char* a,const char* b){
    size_t pos=0;while(*a){if(pos+1<n)p[pos]=*a;++pos;++a;}
    while(*b){if(pos+1<n)p[pos]=*b;++pos;++b;}
    if(n)p[pos<n?pos:n-1]=0;return (int)pos;
}
static FILE* fopen(const char*,const char* mode){
    if(fail_open)return nullptr;if(mode[0]=='w')disk_size=0;file.pos=0;return &file;
}
static int fseek(FILE* f,long pos,int origin){
    if(fail_seek)return -1;f->pos=origin==SEEK_END ? disk_size+pos : pos;return 0;
}
static long ftell(FILE* f){return fail_tell ? -1 : f->pos;}
static size_t fread(char* out,size_t,size_t n,FILE* f){
    if(read_calls++==fail_read_at)return 0;
    if(append_on_read){disk[disk_size++]='X';append_on_read=false;}
    size_t count=0;while(count<n && f->pos<disk_size)out[count++]=disk[f->pos++];return count;
}
static int fputs(const char* p,FILE*){
    if(fail_write)return EOF;++writes;while(*p)disk[disk_size++]=*p++;return 0;
}
static int fflush(FILE*){return fail_flush?-1:0;}
static int fclose(FILE*){++closes;return fail_close?-1:0;}
'''
code += r'''
static int critical_depth, critical_violation, register_result, unregister_result;
#define taskENTER_CRITICAL(x) (++critical_depth)
#define taskEXIT_CRITICAL(x) (--critical_depth)
namespace std {
class mutex {public: void lock(){} void unlock(){}};
template<class T> class lock_guard {T& value;public: lock_guard(T& v):value(v){value.lock();}~lock_guard(){value.unlock();}};
template<class T> const T& min(const T& a,const T& b){return b<a ? b : a;}
}
class SD_CARD {
public:
    std::mutex file_mutex;
    static constexpr char MOUNT_POINT[]="/sdcard";
    esp_err_t write_data(const char*,size_t,char*,char);
    esp_err_t open_sensor_log(FILE**,long*);
    esp_err_t read_sensor_log(FILE*,char*,size_t);
    void close_sensor_log(FILE*);
};
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
'''
for header in ['components/button/button_debounce.h', 'main/button_wifi_session.h']:
    code += '\n' + '\n'.join(line for line in (root / header).read_text(encoding='utf-8').splitlines()
                              if not line.startswith('#')) + '\n'
code += r'''
typedef int gpio_num_t;
#define GPIO_NUM_4 4
static int input_position;
static bool input_levels[16];
static int sent_events;
class BUTTON {
public:
    enum class EventType{PRESSED};
    struct Event{EventType type;gpio_num_t pin;TickType_t tick;};
    struct Input{int read(){return input_levels[input_position++];}} input;
    button_control::DebouncedButton debounce{40};
    QueueHandle_t event_queue=(void*)1;
    gpio_num_t input_pin=4;
    void monitor_task();
};
static BUTTON::Event published_event;
static int xQueueSend(void*,const BUTTON::Event* event,unsigned timeout){
    ++sent_events;published_event=*event;return pdTRUE;
}
static void vTaskDelayUntil(TickType_t* last,unsigned interval){*last+=interval;tick=*last;}
struct httpd_req_t{void* user_ctx;};
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
static int response_status,chunk_count,fail_send_at=-1;
static bool completed;
static char response[4096];static size_t response_size;
static void httpd_resp_set_status(httpd_req_t*,const char*){response_status=503;}
static void httpd_resp_set_type(httpd_req_t*,const char*){}
static void httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){}
static int httpd_resp_sendstr(httpd_req_t*,const char*){return 0;}
static int httpd_resp_send_err(httpd_req_t*,int status,const char*){response_status=status;return 0;}
static int httpd_resp_send_chunk(httpd_req_t*,const char* data,size_t n){
    if(!data){completed=true;return 0;}
    if(chunk_count++==fail_send_at)return ESP_FAIL;
    for(size_t i=0;i<n;++i)response[response_size++]=data[i];return 0;
}
static bool coordinator_read_ok=true;
static int queue_resets;
static void xQueueReset(void*){++queue_resets;}
static int xQueueReceive(void* queue,void* data,unsigned){
    if(!coordinator_read_ok)return 0;
    if(queue==(void*)3)*static_cast<uint16_t*>(data)=812;
    else *static_cast<BME280::results_data_t*>(data)={24.5f,45.0,1018.0f};
    return pdTRUE;
}
namespace std {
typedef long long time_t;
static time_t time(void*){return 12345;}
static bool isfinite(double value){return value==value && value < 1e300 && value > -1e300;}
}

struct SensorData {
  uint16_t co2 {0};
  float temperature {0};
  double humidity {0};
  float pressure {0};
  bool co2_valid {false};
  bool bme280_valid {false};
  std::time_t measured_at {0};
  bool time_valid {false};
  uint32_t sequence {0};
  bool bme_valid() const {
    return bme280_valid && std::isfinite(temperature) &&
        std::isfinite(humidity) && std::isfinite(pressure);
  }
  bool all_valid() const { return co2_valid && bme_valid(); }
  void complete(bool co2_ok, uint16_t ppm, bool bme_ok, float temp,
                double rh, float hpa, std::time_t when, bool clock_valid){
    co2_valid = co2_ok;
    bme280_valid = bme_ok && std::isfinite(temp) && std::isfinite(rh) && std::isfinite(hpa);
    if(co2_valid){ co2 = ppm; }
    if(bme280_valid){ temperature = temp; humidity = rh; pressure = hpa; }
    measured_at = when;
    time_valid = clock_valid;
    ++sequence;
  }
};
class SNTP {public: static bool is_time_synchronized(){return true;}};
class SENSOR_RECORDER {public: enum class Result{SUCCESS, SKIPPED, FAILED};
  Result record(const SensorData&,bool,SD_CARD&){return Result::FAILED;}
};
class SMART_CLOCK {
public:
    struct Flag{bool value=false;bool load(){return value;}} application_ready;
    struct Handle{void* value=nullptr;void* load(){return value;}} update_display_handle;
    static void notify_display_update(void*);
    SensorData sensor_data;
    std::mutex sensor_mutex, notification_mutex;
    SNTP sntp; SENSOR_RECORDER recorder;
    SCD40 scd40; BME280 bme280;
    void* co2_buffer=(void*)3;void* bme280_results_buffer=(void*)4;
    static constexpr unsigned UPDATE_DISPLAY_INTERVAL=60000;
    void monitor_sensor_task();
    std::mutex session_mutex;
    smart_clock_control::DownloadWindow download_window{300000};
    SD_CARD sd_card;bool sd_ready=true;
    void handle_button_event(const BUTTON::Event&);
};
static void handler(){}
'''
for signature in ['esp_err_t GpioInput::set_event_handler(esp_event_handler_t',
                  'esp_err_t GpioInput::set_event_handler(esp_event_loop_handle_t',
                  'void GpioInput::set_queue_handle', 'esp_err_t GpioInput::mclear_event_handlers']:
    code += '\nnamespace GpioInterface {\n' + method('components/gpio/gpio_input.cpp', signature) + '\n}\n'
for signature in ['void SMART_CLOCK::notify_display_update']:
    code += '\n' + method('main/smart_clock.cpp', signature) + '\n'
code += '\n' + method('main/smart_clock.cpp', 'void SMART_CLOCK::handle_button_event') + '\n'
code += r'''
struct SensorHttpAccess {
  void* context;
  SensorData (*snapshot)(void*);
  bool (*begin_request)(void*);
  void (*end_request)(void*);
  bool (*sd_available)(void*);
  SD_CARD* sd;
};
static bool begin_http(void* p){auto* c=static_cast<SMART_CLOCK*>(p);return c->download_window.begin_request(tick);}
static void end_http(void* p){static_cast<SMART_CLOCK*>(p)->download_window.end_request(tick);}
static bool sd_available(void* p){return static_cast<SMART_CLOCK*>(p)->sd_ready;}
namespace sensor_http {
class HttpActivity {
  SensorHttpAccess& access;
public:
  bool admitted;
  explicit HttpActivity(SensorHttpAccess& value) : access(value), admitted(value.begin_request(value.context)) {}
  ~HttpActivity(){ if(admitted){ access.end_request(access.context); } }
};
'''
code += method('main/sensor_http.cpp', 'esp_err_t download_sensor_log') + '\n}\n'
code += '\n' + method('main/smart_clock.cpp', 'void SMART_CLOCK::monitor_sensor_task').replace('while(true)', 'for(int sample=0;sample<1;++sample)', 1) + '\n'
code += '\n' + method('components/button/button.cpp', 'void BUTTON::monitor_task').replace(
    'while(true)', 'for(int sample=0;sample<16;++sample)', 1) + '\n'
for signature in ['esp_err_t I2C::read_byte', 'esp_err_t I2C::read_data', 'esp_err_t I2C::write_byte']:
    code += '\nnamespace i2c_base {\n' + method('components/i2c/i2c_base.cpp', signature) + '\n}\n'
for path, signature in [
    ('components/bme280/bme280.cpp', 'void BME280::measure_task'),
    ('components/scd40/scd40.cpp', 'void SCD40::measure_co2_task'),
    ('components/display/e_paper4in26.cpp', 'esp_err_t EPAPER4IN26::wait_until_ready'),
    ('components/sd_card/sd_card.cpp', 'esp_err_t SD_CARD::write_data'),
    ('components/sd_card/sd_card.cpp', 'esp_err_t SD_CARD::open_sensor_log'),
    ('components/sd_card/sd_card.cpp', 'esp_err_t SD_CARD::read_sensor_log'),
    ('components/sd_card/sd_card.cpp', 'void SD_CARD::close_sensor_log'),
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
    SMART_CLOCK::notify_display_update(&clock);SMART_CLOCK::notify_display_update(&clock);
    if(display_notifications)return 26;
    clock.application_ready.value=true;
    SMART_CLOCK::notify_display_update(&clock);
    if(display_notifications!=1)return 27;
    clock.update_display_handle.value=nullptr;SMART_CLOCK::notify_display_update(&clock);
    if(display_notifications!=1)return 28;
    button_control::DebouncedButton button(40);
    if(button.update(false,0) || button.update(true,10) || button.update(false,20) ||
       button.update(true,30) || button.update(true,69))return 29;
    if(!button.update(true,70) || button.update(true,1000))return 30;
    if(button.update(false,1010) || button.update(false,1050) || button.update(true,1060) ||
       !button.update(true,1100))return 31;
    button_control::DebouncedButton wrapped_button(40);
    if(wrapped_button.update(true,0xfffffff0u) || !wrapped_button.update(true,24))return 32;
    smart_clock_control::DownloadWindow window(300);
    if(window.begin_request(0) || window.expire(500))return 33;
    window.open(100);
    if(window.expire(399) || !window.begin_request(399) || window.expire(2000))return 34;
    window.end_request(2000);
    if(window.expire(2299) || !window.expire(2300) || window.enabled() || window.begin_request(2300))return 35;
    window.open(3000);window.open(3250);
    if(window.expire(3300) || !window.expire(3550))return 36;
    window.open(0xffffff80u);
    if(window.expire(171) || !window.expire(172))return 37;
    fail_open=false;FILE* opened=nullptr;long snapshot=0;
    if(sd.open_sensor_log(nullptr,&snapshot)!=ESP_ERR_INVALID_ARG)return 38;
    fail_seek=true;closed=closes;
    if(sd.open_sensor_log(&opened,&snapshot)!=ESP_FAIL || opened || closes!=closed+1)return 39;
    fail_seek=false;fail_tell=true;
    if(sd.open_sensor_log(&opened,&snapshot)!=ESP_FAIL || opened)return 40;
    fail_tell=false;fail_open=true;errno=ENOENT;
    if(sd.open_sensor_log(&opened,&snapshot)!=ESP_ERR_NOT_FOUND)return 41;
    fail_open=false;disk_size=2500;
    for(long i=0;i<disk_size;++i)disk[i]=(char)(i%127);
    read_calls=0;append_on_read=true;response_size=0;chunk_count=0;completed=false;response_status=0;
    SensorHttpAccess access{&clock,nullptr,begin_http,end_http,sd_available,&clock.sd_card};
    httpd_req_t request{&access};clock.download_window.open(tick);
    closed=closes;
    if(sensor_http::download_sensor_log(&request)!=0 || response_size!=2500 || chunk_count!=3 ||
       !completed || closes!=closed+1 || disk_size!=2501)return 42;
    for(size_t i=0;i<response_size;++i)if(response[i]!=(char)(i%127))return 43;
    fail_send_at=1;response_size=0;chunk_count=0;completed=false;closed=closes;
    if(sensor_http::download_sensor_log(&request)!=ESP_FAIL || completed || response_size!=1024 || closes!=closed+1)return 44;
    fail_send_at=-1;fail_read_at=1;read_calls=0;response_size=0;chunk_count=0;completed=false;
    if(sensor_http::download_sensor_log(&request)!=ESP_FAIL || completed || response_size!=1024)return 45;
    fail_read_at=0;read_calls=0;response_status=0;
    if(sensor_http::download_sensor_log(&request)!=0 || response_status!=500)return 46;
    fail_read_at=-1;clock.sd_ready=false;response_status=0;
    if(sensor_http::download_sensor_log(&request)!=0 || response_status!=503)return 47;
    clock.sd_ready=true;clock.download_window.expire(tick+300000);response_status=0;
    if(sensor_http::download_sensor_log(&request)!=0 || response_status!=503)return 48;
    // Exercise the component task's actual sampling and event publication.
    BUTTON component;tick=0;input_position=sent_events=0;
    const bool levels[16]={false,true,false,true,true,true,true,true,false,false,false,true,true,true,true,true};
    for(int i=0;i<16;++i)input_levels[i]=levels[i];
    component.monitor_task();
    if(sent_events!=2 || published_event.pin!=4 || published_event.type!=BUTTON::EventType::PRESSED ||
       published_event.tick!=260)return 49;
    // The coordinator must reject events before readiness or from another pin.
    BUTTON::Event press{BUTTON::EventType::PRESSED,4,tick};
    clock.application_ready.value=false;clock.handle_button_event(press);
    if(clock.download_window.enabled())return 50;
    clock.application_ready.value=true;press.pin=5;clock.handle_button_event(press);
    if(clock.download_window.enabled())return 51;
    press.pin=4;clock.update_display_handle.value=(void*)1;
    int prior_notifications=display_notifications;clock.handle_button_event(press);
    if(!clock.download_window.enabled() || display_notifications!=prior_notifications+1)return 52;
    tick+=200000;clock.handle_button_event(press);
    if(clock.download_window.expire(tick+299999) || !clock.download_window.expire(tick+300000))return 53;
    clock.application_ready.value=true;clock.update_display_handle.value=(void*)1;
    prior_notifications=display_notifications;queue_resets=0;coordinator_read_ok=true;
    clock.monitor_sensor_task();
    if(!clock.sensor_data.all_valid() || clock.sensor_data.co2!=812 || clock.sensor_data.sequence!=1 || queue_resets!=2 || display_notifications!=prior_notifications+1)return 54;
    coordinator_read_ok=false;clock.monitor_sensor_task();
    if(clock.sensor_data.all_valid() || clock.sensor_data.co2!=812 || clock.sensor_data.sequence!=2 || display_notifications!=prior_notifications+2)return 55;
    clock.application_ready.value=false;coordinator_read_ok=true;clock.monitor_sensor_task();
    if(!clock.sensor_data.all_valid() || display_notifications!=prior_notifications+2)return 56;
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
print('PASS: 56 regression checks (I2C, sensors, BUSY, CSV, GPIO, button event delivery, idle timeout, HTTP failures)')
