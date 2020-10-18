import requests
from requests.auth import HTTPBasicAuth

ip = "192.168.0.1"
port = "9999"
api = "v1"
client_id = 1
client_transaction_id = 1234
client_dict = {"ClientID" : client_id, "ClientTransactionID" : client_transaction_id}

def print_response(response):
    print(response, response.text)

def get_url(*parts):
    print("http://" + "/".join(str(p) for p in parts))
    return "http://" + "/".join(str(p) for p in parts)

def management_get(method, versioned):
    url = get_url(ip, "management", api, method) if versioned else get_url(ip, "management", method)
    r = requests.get(url, params=client_dict)
    return r

def device_get(device_type, device_number, method):
    url = get_url(ip, "api", api, device_type, device_number, method)
    params = {"ClientID" : client_id, "ClientTransactionID" : client_transaction_id}
    r = requests.get(url, params=client_dict)
    return r

def device_put(device_type, device_number, method, **kwargs):
    url = get_url(ip, "api", api, device_type, device_number, method)
    json = {**client_dict, **kwargs}
    r = requests.put(url, json=json)
    return r


print_response(management_get("apiversions", False))
print_response(management_get("description", True))
print_response(management_get("configureddevices", True))

# success (200): correct url
print_response(device_get("telescope", 0, "targetdeclination"))
# fail (400): url is case sensitive
print_response(device_get("telescOpe", 0, "taRgetdEclination"))
# fail (400): url misspelled
print_response(device_get("telescoe", 0, "targetdeclination"))
# fail (400): url misspelled
print_response(device_get("telescope", 0, "targetdelination"))
# fail (400): invalid device number
print_response(device_get("telescope", 1, "targetdeclination"))

# test put + get
print_response(device_put("telescope", 0, "targetdeclination", TargetDeclination=52.73))
print_response(device_get("telescope", 0, "targetdeclination"))
