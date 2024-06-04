export const iot_api = 'api';

// 对ajax的Promise封装
export const ajax = function(url,type,data,datatype){
    return new Promise((resolve,reject)=>{
        $.ajax({
            type: type,
            url: url,
            data: data,
            dataType: datatype,
            success: function (res) {
                resolve(res)
            },
            error:function(error){
                reject(error)
            }
        });
    })
}

// 登录接口
export const login_request = (param) => ajax(`${iot_api}/login.action`,'POST',param,'json')

// 获取ap参数接口
export const get_ap_request = () => ajax(`${iot_api}/ap.action`,'GET', {},'json')

// 设置ap参数接口
export const set_ap_request = (param) => ajax(`${iot_api}/ap.action`,'POST',param,'json')

// 获取采集数据接口（部分机型才有）
export const get_collect_request = () => ajax(`${iot_api}/collect.action`,'GET', {},'json')

// 获取主页面板数据接口
export const get_home_request = () => ajax(`${iot_api}/home.action`,'GET', {},'json')

// 获取led接口
export const get_led_request = () => ajax(`${iot_api}/led.action`,'GET', {},'json')

// 设置led接口
export const set_led_request = (param) => ajax(`${iot_api}/led.action`,'POST',param,'json')

// 获取mqtt参数接口
export const get_mqtt_request = () => ajax(`${iot_api}/mqtt.action`,'GET', {},'json')

// 设置mqtt参数接口
export const set_mqtt_request = (param) => ajax(`${iot_api}/mqtt.action`,'POST',param,'json')

// 设置密码接口
export const set_pwd_request = (param) => ajax(`${iot_api}/pwd.action`,'POST',param,'json')

// 获取sta参数接口
export const get_sta_request = () => ajax(`${iot_api}/sta.action`,'GET', {},'json')

// 设置sta参数接口
export const set_sta_request = (param) => ajax(`${iot_api}/sta.action`,'POST',param,'json')