"""Host fault-injection checks for architecture changes. No device access."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--zig', type=Path, required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = root / 'build/architecture-tests'
out.mkdir(parents=True, exist_ok=True)

def stripped(path):
    return '\n'.join(line for line in (root / path).read_text(encoding='utf-8').splitlines()
                     if not line.startswith('#include') and not line.startswith('#pragma'))

def method(path, signature):
    text = (root / path).read_text(encoding='utf-8')
    start = text.index(signature)
    brace = text.index('{', start)
    depth, end = 1, brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]

code = r'''
#include <cstdio>
#include <ctime>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <atomic>
#include <mutex>
#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cstdarg>
static std::tm* localtime_r(const std::time_t* value,std::tm* result){auto* local=std::localtime(value);if(!local)return nullptr;*result=*local;return result;}
static int sensor_lock_depth;
class TrackingMutex { public: void lock(){++sensor_lock_depth;} void unlock(){--sensor_lock_depth;} };
using esp_err_t = int;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 259
#define ESP_ERR_NO_MEM 257
#define ESP_ERR_TIMEOUT 263
#define ESP_ERR_NOT_FOUND 261
#define ESP_LOGI(...)
#define ESP_LOGW(...)
#define ESP_LOGE(...)
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define portMAX_DELAY 0xffffffffu
#define pdMS_TO_TICKS(x) (x)
#define GPIO_NUM_4 4
#define GPIO_NUM_NC -1
static void gpio_reset_pin(int){}
#define HTTP_GET 0
#define IP_EVENT 1
#define IP_EVENT_STA_GOT_IP 1
using esp_event_base_t = int;
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
static int checks;
#define CHECK(condition) do { ++checks; if(!(condition)){ std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); return 1; } } while(0)
struct Gate { bool released=false; };
using EventGroupHandle_t = Gate*;
struct Task { void (*entry)(void*); void* arg; int notifications=0; };
using TaskHandle_t = Task*;
using QueueHandle_t = void*;
static std::set<Task*> tasks;
static std::set<void*> queues;
static std::set<Gate*> gates;
static int op, fail_op=-1, task_op, fail_task=-1, normal_work, pending_notifications;
static uint32_t tick;
static int64_t monotonic;
static bool step(){return ++op!=fail_op;}
struct Blocked {};
static void xEventGroupWaitBits(Gate* gate,int,int,int,unsigned){ if(!gate || !gate->released) throw Blocked{}; }
static Gate* xEventGroupCreate(){if(!step())return nullptr;auto* g=new Gate;gates.insert(g);return g;}
static void vEventGroupDelete(Gate* g){if(gates.erase(g))delete g;}
static void xEventGroupSetBits(Gate* g,int){g->released=true;}
static QueueHandle_t xQueueCreate(int,size_t){if(!step())return nullptr;auto* q=new int;queues.insert(q);return q;}
static void vQueueDelete(void* q){if(queues.erase(q))delete static_cast<int*>(q);}
static int xTaskCreate(void(*entry)(void*),const char*,unsigned,void* arg,unsigned,Task** handle){
  if(++task_op==fail_task)return 0;
  auto* t=new Task{entry,arg};tasks.insert(t);*handle=t;
  try{entry(arg);}catch(Blocked&){} // The scheduler may run each task immediately during creation.
  return 1;
}
static void vTaskDelete(Task* t){if(tasks.erase(t))delete t;}
static void xTaskNotifyGive(Task* t){if(!tasks.count(t))throw std::runtime_error("notify deleted task");++t->notifications;++pending_notifications;}
static unsigned ulTaskNotifyTake(int,unsigned){return 1;}
static TickType_t xTaskGetTickCount(){return tick;}
static int64_t esp_timer_get_time(){return monotonic;}
static void vTaskDelayUntil(TickType_t* last,unsigned n){*last+=n;tick=*last;}
static void vTaskDelay(unsigned n){tick+=n;}
static void xQueueReset(void*){}
static int i2c_master_bus_rm_device(void*){return 0;}
static void esp_log_level_set(const char*,int){}
#define ESP_LOG_INFO 1
static bool event_loop, nvs, event_handler;
static int esp_event_loop_create_default(){if(!step())return ESP_FAIL;event_loop=true;return 0;}
static void esp_event_loop_delete_default(){event_loop=false;}
static int nvs_flash_init(){if(!step())return ESP_FAIL;nvs=true;return 0;}
static void nvs_flash_deinit(){nvs=false;}
static int esp_event_handler_register(int,int,void(*)(void*,int,int,void*),void*){if(!step())return ESP_FAIL;event_handler=true;return 0;}
static void esp_event_handler_unregister(int,int,void(*)(void*,int,int,void*)){event_handler=false;}
namespace i2c_base { class I2C {public: bool ready=false; int init(){ready=step();return ready?0:-1;} void deinit(){ready=false;} }; }
class SD_CARD {
public:
  bool ready=false, fail_write=false, absent=false;
  int attempts=0;std::vector<std::string> rows; std::string header;
  int init(){if(absent)return -1;ready=step();return ready?0:-1;}
  void deinit(){ready=false;}
  int write_data(const char*,size_t,char* text,char mode){
    if(mode=='h'){header=text;return 0;}
    if(sensor_lock_depth)throw std::runtime_error("SD access under sensor mutex");
    ++attempts;if(fail_write)return -1; rows.push_back(text);return 0;
  }
  int open_sensor_log(FILE**,long*){return -1;}
  int read_sensor_log(FILE*,char*,size_t){return -1;}
  void close_sensor_log(FILE*){}
};
'''
code += stripped('components/sensor_data/sensor_data.h')
code += stripped('components/sensor_recorder/sensor_recorder.h')
code += stripped('components/sensor_recorder/sensor_recorder.cpp')
code += stripped('main/button_wifi_session.h')
code += stripped('main/sync_schedule.h')
code += r'''
static bool co2_ok=true,bme_ok=true,bme_nan=false;
class BME280 {
public:
  struct results_data_t{float temperature;double humidity;float pressure;};
  void* results_buffer=nullptr; void* mi2c_device_handle=nullptr;void* pmi2c=nullptr;
  Task* task_handle=nullptr;Gate* startup_gate=nullptr;
  int init(i2c_base::I2C*){return step()?0:-1;}
  QueueHandle_t get_results_buffer(){return results_buffer;}
  void notify_measurement_start(){}
  int prepare_queue();int create_task(const char*,uint16_t,unsigned,Gate*);void deinit();
  static void get_measure_task_entry_point(void*);
  void measure_task(){++normal_work;}
};
class SCD40 {
public:
  void* co2_buffer=nullptr;void* mi2c_device_handle=nullptr;void* pmi2c=nullptr;
  Task* task_handle=nullptr;Gate* startup_gate=nullptr;
  int init(i2c_base::I2C*){return step()?0:-1;}
  QueueHandle_t get_co2_buffer_handle(){return co2_buffer;}
  void notify_measurement_start(){}
  int prepare_queue();int create_task(const char*,uint16_t,unsigned,Gate*);void deinit();
  static void get_measure_co2_task_entry_point(void*);
  void measure_co2_task(){++normal_work;}
};
class BUTTON {
public:
  enum class EventType{PRESSED};struct Event{EventType type;int pin;uint32_t tick;};
  void* event_queue=nullptr;Task* task_handle=nullptr;Gate* startup_gate=nullptr;
  int input_pin=-1;
  int init(int,void* queue){event_queue=queue;return step()?0:-1;}
  int create_task(const char*,uint16_t,unsigned,Gate*);void deinit();
  static void task_entry(void*);void monitor_task(){++normal_work;}
};
static void* co2_queue;
static int xQueueReceive(void* queue,void* result,unsigned){
  if(queue==co2_queue){if(!co2_ok)return 0;*static_cast<uint16_t*>(result)=812;}
  else{if(!bme_ok)return 0;*static_cast<BME280::results_data_t*>(result)={bme_nan?std::numeric_limits<float>::quiet_NaN():24.5f,45.0,1018.0f};}
  return 1;
}
class WIFI {
public:
  enum class state_e{NOT_INITIALIZED,INITIALIZED,READY_TO_CONNECT,CONNECTING,WAITING_FOR_IP,CONNECTED,DISCONNECTED,PROVISIONING,ERROR};
  state_e state=state_e::NOT_INITIALIZED;int stops=0,starts=0,begins=0;
  int init(){if(!step())return -1;state=state_e::INITIALIZED;return 0;}
  void deinit(){state=state_e::NOT_INITIALIZED;}
  void process_events(){}
  state_e get_state(){return state;}
  int start(){++starts;state=state_e::READY_TO_CONNECT;return 0;}
  int stop(){++stops;state=state_e::INITIALIZED;return 0;}
  int begin(){++begins;state=state_e::CONNECTING;return 0;}
};
class SNTP {
public:
  bool synchronized=true,running=false;void(*callback)(void*)=nullptr;void* context=nullptr;
  bool is_time_synchronized(){return synchronized;}
  bool is_running(){return running;}
  int init(){running=true;return 0;}
  bool set_update_interval(unsigned,bool){return true;}
  void stop(){running=false;}
  void set_time_sync_callback(void(*cb)(void*),void* ctx){callback=cb;context=ctx;}
};
static std::string drawn;
namespace fonts {static int Font4,Font8,FreeSans24pt7b,lgfxJapanGothicP_24;}
class LGFX_Sprite {
public:
  void setColorDepth(int){}void* createSprite(int,int){return this;}void deleteSprite(){}
  void setTextWrap(bool){}void setTextColor(int){}void setFont(const int*){}void setTextSize(double){}
  void setCursor(int,int){}int getCursorY(){return 0;}int getCursorX(){return 0;}int width(){return 800;}
  int textWidth(const char*){return 1;}int fontHeight(){return 20;}void* getBuffer(){return this;}
  void fillScreen(int){if(sensor_lock_depth)throw std::runtime_error("draw under sensor mutex");drawn.clear();}
  void print(const char* text){drawn+=text;}void println(const char* text){drawn+=text;drawn+='\n';}
  void printf(const char* format,...){char buffer[256];va_list args;va_start(args,format);vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);drawn+=buffer;}
};
class EPAPER4IN26 {
public:
  enum class state_e{NOT_INITIALIZED,INITIALIZED,SLEEP};state_e state=state_e::INITIALIZED;
  int init(){return step()?0:-1;}void deinit(){}int init_epaper(){return 0;}
  int get_display_resolution_width(){return 800;}int get_display_resolution_height(){return 480;}
  int get_display_bytes(){return 48000;}state_e get_state(){return state;}
  int display(const uint8_t*,size_t){return 0;}int set_sleep_mode(){state=state_e::SLEEP;return 0;}
};
'''
code += stripped('components/clock_display/clock_display.h')
code += stripped('components/clock_display/clock_display.cpp')
code += r'''
using httpd_handle_t = void*;
struct httpd_req_t {void* user_ctx;};
struct httpd_uri_t{const char* uri;int method;int(*handler)(httpd_req_t*);void* user_ctx;};
static int response_status=200;static std::string body,query;static SensorData* change_during_wait;
static int requests,ends;static bool access_allowed=true,sd_allowed=true;
#define HTTPD_404_NOT_FOUND 404
#define HTTPD_500_INTERNAL_SERVER_ERROR 500
static void httpd_resp_set_status(httpd_req_t*,const char* status){response_status=atoi(status);}
static void httpd_resp_set_type(httpd_req_t*,const char*){}
static void httpd_resp_set_hdr(httpd_req_t*,const char*,const char*){}
static int httpd_resp_sendstr(httpd_req_t*,const char* text){if(sensor_lock_depth)throw std::runtime_error("HTTP under sensor mutex");body=text;return 0;}
static int httpd_resp_send(httpd_req_t*,const char*,size_t){body.clear();return 0;}
static int httpd_resp_send_chunk(httpd_req_t*,const char*,size_t){return 0;}
static int httpd_resp_send_err(httpd_req_t*,int,const char*){return 0;}
static int httpd_req_get_url_query_str(httpd_req_t*,char* text,size_t n){if(query.empty())return -1;snprintf(text,n,"%s",query.c_str());return 0;}
static int httpd_query_key_value(const char* text,const char*,char* value,size_t n){if(strncmp(text,"since=",6))return -1;snprintf(value,n,"%s",text+6);if(change_during_wait)++change_during_wait->sequence;return 0;}
static std::vector<std::string> routes;
static int httpd_register_uri_handler(void*,httpd_uri_t* uri){routes.emplace_back(uri->uri);return 0;}
static void httpd_stop(void*){}
'''
code += stripped('main/sensor_http.h')
code += stripped('main/sensor_http.cpp')
code += stripped('main/smart_clock.h').replace('  private:', '  public:').replace('std::mutex sensor_mutex;', 'TrackingMutex sensor_mutex;').replace('std::lock_guard<std::mutex> guard(sensor_mutex);', 'std::lock_guard<TrackingMutex> guard(sensor_mutex);')
# Methods use exact production bodies; only unbounded loops are limited in the harness.
for sig in ['SMART_CLOCK::SMART_CLOCK', 'void SMART_CLOCK::notify_display_update',
            'esp_err_t SMART_CLOCK::init', 'void SMART_CLOCK::cleanup', 'esp_err_t SMART_CLOCK::run',
            'esp_err_t SMART_CLOCK::create_monitor_sensor_task', 'esp_err_t SMART_CLOCK::create_update_display_task',
            'void SMART_CLOCK::get_monitor_sensor_task_entry_point', 'void SMART_CLOCK::get_update_display_task_entry_point',
            'void SMART_CLOCK::network_task_entry', 'void SMART_CLOCK::wifi_connected_event_handler',
            'void SMART_CLOCK::sntp_time_sync_complete', 'void SMART_CLOCK::handle_button_event',
            'void SMART_CLOCK::wifi_run', 'SensorData SMART_CLOCK::http_snapshot', 'bool SMART_CLOCK::http_begin',
            'void SMART_CLOCK::http_end', 'bool SMART_CLOCK::http_sd_available', 'void SMART_CLOCK::monitor_sensor_task',
            'void SMART_CLOCK::update_display_task']:
    code += '\n' + method('main/smart_clock.cpp', sig).replace('while(true)', 'for(int iteration=0;iteration<1;++iteration)', 1).replace('std::lock_guard<std::mutex> guard(sensor_mutex);', 'std::lock_guard<TrackingMutex> guard(sensor_mutex);')
code += r'''
CLOCK_DISPLAY::Context SMART_CLOCK::display_context(){return {};}
esp_err_t SMART_CLOCK::start_data_server(){data_server=(void*)1;return 0;}
void SMART_CLOCK::network_task(){++normal_work;}
'''
for cls, folder, entry in [('BME280','bme280','get_measure_task_entry_point'),('SCD40','scd40','get_measure_co2_task_entry_point')]:
    for sig in [f'esp_err_t {cls}::prepare_queue',f'esp_err_t {cls}::create_task',f'void {cls}::deinit',f'void {cls}::{entry}']:
        code += '\n' + method(f'components/{folder}/{folder}.cpp',sig)
for sig in ['esp_err_t BUTTON::create_task','void BUTTON::deinit','void BUTTON::task_entry']:
    code += '\n' + method('components/button/button.cpp',sig)
code += r'''
static SensorData http_sample;
static SensorData snapshot(void*){return http_sample;}
static bool begin(void*){++requests;return access_allowed;}
static void end(void*){++ends;}
static bool sd_available(void*){return sd_allowed;}
static void reset_faults(){op=task_op=normal_work=pending_notifications=0;fail_op=fail_task=-1;tick=0;monotonic=0;}
int main(){
  SensorData data;
  CHECK(!data.all_valid() && !data.co2_valid && !data.bme_valid());
  data.complete(true,812,true,24.5,45,1018,1720000000,true);
  CHECK(data.all_valid() && data.sequence==1 && data.measured_at==1720000000);
  data.complete(false,0,true,25,46,1020,1720000060,true);
  CHECK(!data.co2_valid && data.bme_valid() && data.temperature==25 && data.co2==812 && data.sequence==2);
  data.complete(true,900,false,0,0,0,1720000120,true);
  CHECK(data.co2_valid && !data.bme_valid() && data.co2==900 && data.temperature==25 && data.sequence==3);
  data.complete(true,901,true,std::numeric_limits<float>::quiet_NaN(),45,1018,0,false);
  CHECK(!data.all_valid() && data.sequence==4 && !data.time_valid);
  data.complete(true,902,true,24,45,std::numeric_limits<float>::infinity(),0,false);
  CHECK(!data.bme_valid());
  SD_CARD sd; SENSOR_RECORDER recorder;
  CHECK(recorder.prepare(sd)==0 && sd.header=="YYYY/MM/DD, week, HH:MM:SS, CO2[ppm], Temperature[degree], Humidity[%], Pressure[hPa]\n");
  data.complete(true,812,true,24.5,45,1018,1720000000,true);
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SUCCESS && sd.attempts==1);
  std::tm local{};localtime_r(&data.measured_at,&local);char timestamp[100];strftime(timestamp,sizeof(timestamp),"%Y/%m/%d, %a, %H:%M:%S",&local);
  CHECK(sd.rows[0]==std::string(timestamp)+", 812, 24.50, 45.00, 1018.00\n");
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED && sd.attempts==1);
  ++data.sequence;sd.fail_write=true;
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::FAILED && sd.attempts==2);
  sd.fail_write=false;
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED && sd.attempts==2);
  ++data.sequence;data.time_valid=false;
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED);
  data.time_valid=true;
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED);
  ++data.sequence;CHECK(recorder.record(data,false,sd)==SENSOR_RECORDER::Result::SKIPPED);
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED);
  ++data.sequence;data.co2_valid=false;
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED);
  data.co2_valid=true;
  CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED && sd.attempts==2);
  --data.sequence;CHECK(recorder.record(data,true,sd)==SENSOR_RECORDER::Result::SKIPPED);
  SENSOR_RECORDER wrap;data.sequence=0xffffffffu;
  CHECK(wrap.record(data,true,sd)==SENSOR_RECORDER::Result::SUCCESS);
  data.sequence=0;CHECK(wrap.record(data,true,sd)==SENSOR_RECORDER::Result::SUCCESS);
  CLOCK_DISPLAY display;CHECK(display.init()==0);
  SensorData empty;CHECK(display.render(empty,{})==0 && drawn.find("CO2  --ppm")!=std::string::npos && drawn.find("--hpa")!=std::string::npos);
  data.bme280_valid=false;data.co2_valid=true;data.co2=812;
  CHECK(display.render(data,{})==0 && drawn.find("CO2  812ppm")!=std::string::npos && drawn.find("--hpa")!=std::string::npos);
  data.co2_valid=false;data.bme280_valid=true;
  CHECK(display.render(data,{})==0 && drawn.find("CO2  --ppm")!=std::string::npos && drawn.find("1018.00hpa")!=std::string::npos);
  CLOCK_DISPLAY::Context provisioning;provisioning.provisioning=true;strcpy(provisioning.setup_ssid,"TEST_AP");strcpy(provisioning.provisioning_pop,"TEST_POP");
  CHECK(display.render(empty,provisioning)==0 && drawn.find("TEST_AP")!=std::string::npos && drawn.find("TEST_POP")!=std::string::npos);
  SensorHttpAccess http{nullptr,snapshot,begin,end,sd_available,&sd};httpd_req_t req{&http};
  http_sample.complete(true,812,true,24.5,45,1018,0,false);response_status=200;
  CHECK(sensor_http::latest_sensor_data(&req)==0 && body=="{\"valid\":true,\"co2_ppm\":812,\"temperature_c\":24.50,\"humidity_percent\":45.00,\"pressure_hpa\":1018.00,\"sequence\":1}" && response_status==200);
  query="since=1";tick=0;
  CHECK(sensor_http::latest_sensor_data(&req)==0 && response_status==204 && body.empty() && tick==1000);
  response_status=200;change_during_wait=&http_sample;
  CHECK(sensor_http::latest_sensor_data(&req)==0 && response_status==200 && body.find("\"sequence\":2")!=std::string::npos);
  change_during_wait=nullptr;query="since=2";http_sample.bme280_valid=false;
  CHECK(sensor_http::latest_sensor_data(&req)==0 && response_status==503 && body=="{\"valid\":false}");
  query.clear();http_sample.bme280_valid=true;http_sample.pressure=std::numeric_limits<float>::infinity();response_status=200;
  CHECK(sensor_http::latest_sensor_data(&req)==0 && response_status==503 && body=="{\"valid\":false}");
  access_allowed=false;response_status=200;
  CHECK(sensor_http::latest_sensor_data(&req)==0 && response_status==503);
  CHECK(requests==ends+1);access_allowed=true;
  CHECK(sensor_http::register_handlers((void*)1,http)==0 && routes==std::vector<std::string>({"/","/sensor_log.csv","/api/sensors/latest"}));
  for(int fail=1;fail<=6;++fail){
    reset_faults();SMART_CLOCK c;CHECK(c.init()==0 && tasks.empty());fail_task=fail;
    CHECK(c.run()!=0 && tasks.empty() && queues.empty() && gates.empty() && !c.application_ready.load() && !event_loop && !nvs && !event_handler && normal_work==0);
    c.cleanup();CHECK(tasks.empty() && queues.empty() && gates.empty());
  }
  // Fail each initialization operation; no task has been created and all acquired queues are reclaimed.
  for(int fail=1;fail<=14;++fail){
    reset_faults();SMART_CLOCK c;fail_op=fail;const int result=c.init();
    if(result==0){c.cleanup();}
    CHECK(tasks.empty() && queues.empty() && gates.empty() && !event_loop && !nvs && !event_handler);
    c.cleanup();CHECK(queues.empty() && gates.empty());
  }
  reset_faults();SMART_CLOCK c;c.sd_card.absent=true;
  CHECK(c.init()==0 && !c.sd_ready && tasks.empty());
  CHECK(c.run()==0 && tasks.size()==6 && c.start_gate->released && normal_work==0 && c.update_display_handle.load()->notifications==1);
  co2_queue=c.co2_buffer;c.sd_ready=true;c.sd_card.fail_write=true;
  c.monitor_sensor_task();CHECK(c.sensor_data.sequence==1 && c.sensor_data.all_valid() && c.sd_card.attempts==1 && c.update_display_handle.load()->notifications==2);
  c.update_display_task();SMART_CLOCK::notify_display_update(&c);c.update_display_task();
  CHECK(c.sd_card.attempts==1);
  co2_ok=false;c.monitor_sensor_task();CHECK(!c.sensor_data.co2_valid && c.sensor_data.bme_valid() && c.sd_card.attempts==1);
  co2_ok=true;bme_ok=false;c.monitor_sensor_task();CHECK(c.sensor_data.co2_valid && !c.sensor_data.bme_valid());
  bme_ok=true;bme_nan=true;c.monitor_sensor_task();CHECK(!c.sensor_data.bme_valid() && c.sensor_data.sequence==4);
  bme_nan=false;c.cleanup();CHECK(tasks.empty() && queues.empty() && gates.empty());
  reset_faults();SMART_CLOCK network;network.application_ready.store(true);network.sntp.synchronized=false;network.wifi.state=WIFI::state_e::INITIALIZED;
  network.wifi_run();CHECK(network.wifi.starts==1 && network.sync_schedule.active());
  monotonic=smart_clock_control::SyncSchedule::TIMEOUT_US;network.wifi_run();CHECK(network.wifi.stops==1 && !network.sync_schedule.active());
  monotonic+=smart_clock_control::SyncSchedule::RETRY_US-1;network.wifi_run();CHECK(network.wifi.starts==1);
  ++monotonic;network.wifi_run();CHECK(network.wifi.starts==2);
  network.wifi.state=WIFI::state_e::CONNECTED;network.wifi_run();CHECK(network.sntp.running);
  network.time_sync_stop_requested.store(true);network.wifi_run();CHECK(network.wifi.stops==2 && !network.sntp.running);
  network.wifi.state=WIFI::state_e::CONNECTED;network.sync_schedule.begin(monotonic);network.download_window.open(tick);
  network.time_sync_stop_requested.store(true);network.wifi_run();CHECK(network.wifi.stops==2 && network.data_server!=nullptr);
  network.sync_schedule.begin(monotonic);monotonic+=smart_clock_control::SyncSchedule::TIMEOUT_US;network.wifi_run();CHECK(network.wifi.stops==2 && network.data_server!=nullptr);
  CHECK(network.download_window.begin_request(tick));tick+=300000;network.wifi_run();CHECK(network.wifi.stops==2);
  network.download_window.end_request(tick);tick+=299999;network.wifi_run();CHECK(network.wifi.stops==2);
  ++tick;network.wifi_run();CHECK(network.wifi.stops==3 && network.data_server==nullptr);
  network.wifi.state=WIFI::state_e::PROVISIONING;network.sync_schedule.begin(monotonic);monotonic+=2*smart_clock_control::SyncSchedule::TIMEOUT_US;network.wifi_run();CHECK(network.wifi.stops==3 && network.sync_schedule.active());
  smart_clock_control::SyncSchedule schedule;CHECK(schedule.due(0,false));schedule.begin(0);CHECK(!schedule.expire(299999999,false));
  CHECK(!schedule.expire(300000000,true) && schedule.active());CHECK(schedule.expire(300000000,false));
  CHECK(!schedule.due(2099999999,true) && schedule.due(2100000000,true));schedule.begin(2100000000);schedule.success();CHECK(!schedule.due(2100000000,false) && schedule.due(2100000000,true));
  // Large monotonic times cross the 32-bit FreeRTOS tick range without affecting deadlines.
  const int64_t long_uptime=1LL<<48;schedule.begin(long_uptime);CHECK(!schedule.expire(long_uptime+299999999,false) && schedule.expire(long_uptime+300000000,false));
  std::printf("PASS: %d architecture checks (snapshot, CSV, display, HTTP, startup rollback, synchronization)\n",checks);
  return 0;
}
'''
source = out / 'architecture.cpp'
source.write_text(code, encoding='utf-8')
exe = out / 'architecture.exe'
subprocess.run([str(args.zig), 'c++', '-std=c++17', '-O0', str(source), '-o', str(exe)], check=True)
subprocess.run([str(exe)], check=True)
