/*
 * Title       OnStep Alpaca
 * by          Lukas Toenne
 *
 * Copyright (C) 2020 Lukas Toenne
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *
 * Revision History, see GitHub
 *
 *
 * Author: Lukas Toenne
 * lukas.toenne@gmail.com
 *
 * Description
 *
 * Implementation of the ASCOM-Alpaca REST API.
 *
 */


#define DEFAULT_JSON_BUFFER 128
#define API_VERSION "v1"

namespace Alpaca
{

  enum ErrorCode
  {
    None = 0,
    // Reserved error code to indicate that the requested action is not implemented in this driver.
    ActionNotImplementedException = 0x8004040C,
    // The starting value for driver-specific error numbers.
    DriverBase = 0x80040500,
    // The maximum value for driver-specific error numbers.
    DriverMax = 0x80040FFF,
    // Reserved error code to indicate that the requested operation can not be undertaken at this time.
    InvalidOperationException = 0x8004040B,
    // Reserved error code for reporting an invalid value.
    InvalidValue = 0x80040401,
    // Reserved error code used to indicate that the attempted operation is invalid because the mount is currently in a Parked state.
    InvalidWhileParked = 0x80040408,
    // Reserved error code used to indicate that the attempted operation is invalid because the mount is currently in a Slaved state.
    InvalidWhileSlaved = 0x80040409,
    // Reserved error code used to indicate that the communications channel is not connected.
    NotConnected = 0x80040407,
    // Reserved error number for property or method not implemented.
    NotImplemented = 0x80040400,
    // Reserved error code to indicate that the requested item is not present in the ASCOM cache.
    NotInCacheException = 0x8004040D,
    // Reserved error code related to settings.
    SettingsProviderError = 0x8004040A,
    // Reserved 'catch-all' error code used when nothing else was specified.
    UnspecifiedError = 0x800404FF,
    // Reserved error code for reporting that a value has not been set. 
    ValueNotSet = 0x80040402,
  };

  int commandErrorToCode(int commandError)
  {
    switch (commandError)
    {
      case CE_NONE : return ErrorCode::None;
      case CE_0 : return ErrorCode::None;
      case CE_NULL : return ErrorCode::UnspecifiedError;

      default: return (int)ErrorCode::DriverBase + commandError - 1;
    }
  }

  const char* commandErrorToString(int commandError)
  {
    switch (commandError)
    {
      case CE_NONE : return "";
      case CE_0 : return "";

      default: return commandErrorToStr(commandError);
    }
  }

  const char ServerName[] = "OnStep WiFi";
  const char Manufacturer[] = "";
  const char ManufacturerVersion[] = "";
  const char Location[] = "";

  void sendInvalidRequestError(const String& errorMessage)
  {
    server.send(400, "application/json", "{\"Value\":\"" + errorMessage + "\"}");
  }

  bool parseArg(const String& argName, String& valueOut)
  {
    String v = server.arg(argName);
    if (v == "")
    {
      sendInvalidRequestError("Parameter '" + argName + "' not set");
      return false;
    }
    valueOut = v;
    return true;
  }

  bool parseArg(const String& argName, int& valueOut)
  {
    String v = server.arg(argName);
    if (v == "")
    {
      sendInvalidRequestError("Parameter '" + argName + "' not set");
      return false;
    }
    if (!atoi2((char*)v.c_str(), &valueOut))
    {
      sendInvalidRequestError("Parameter '" + argName + "' must be int");
      return false;
    }
    return true;
  }

  class AlpacaResponse
  {
  public:
    AlpacaResponse()
      : jsonParams(DynamicJsonDocument(DEFAULT_JSON_BUFFER))
      , jsonBody(DynamicJsonDocument(DEFAULT_JSON_BUFFER))
    {
    }

    bool init()
    {
      switch (server.method())
      {
        case HTTP_GET:
        {
          if (!parseArg("ClientID", clientID))
          {
            // TODO optional/required?
            // return false;
          }
          if (!parseArg("ClientTransactionID", clientTransactionID))
          {
            // TODO optional/required?
            // return false;
          }
          break;
        }

        case HTTP_PUT:
        {
          if (!server.hasArg("plain"))
          {
            sendInvalidRequestError("Body not received");
            return false;
          }

          jsonParamsBuffer = server.arg("plain");
          // Warning: this performs a no-copy deserialization, using string pointers into the jsonParamsBuffer.
          // jsonParams becomes invalid if the string is modified.
          DeserializationError result = deserializeJson(jsonParams, jsonParamsBuffer.c_str());
          if (result != DeserializationError::Ok)
          {
            sendInvalidRequestError(result.c_str());
            return false;
          }

          if (!parseJsonArg("ClientID", clientID))
          {
            // TODO optional/required?
            // return false;
          }
          if (!parseJsonArg("ClientTransactionID", clientTransactionID))
          {
            // TODO optional/required?
            // return false;
          }
          break;
        }

        default:
          sendInvalidRequestError("Method '" + String(server.method()) + "' not implemented");
          return false;
      }

      return true;
    }

    void setError(int _errorCode, const String& _errorMessage)
    {
      errorCode = _errorCode;
      errorMessage = _errorMessage;
    }

    template<typename T>
    bool parseJsonArg(const String& argName, T& valueOut)
    {
      JsonVariantConst jsonVar = jsonParams.getMember(argName);
      if (jsonVar.isNull())
      {
        sendInvalidRequestError("Body does not contain field '" + argName + "'");
        return false;
      }
      if (!jsonVar.is<T>())
      {
        sendInvalidRequestError("Field '" + argName + "' has incorrect type");
        return false;
      }
      valueOut = jsonVar.as<T>();
      return true;
    }

    bool executeCommand(const char* cmd, char* result)
    {
      if (!command(cmd, result))
      {
        char err[60];
        sprintf(err, "Command %s failed", cmd);
        setError(ErrorCode::UnspecifiedError, err);
        return false;
      }
      return true;
    }

    bool executeCommandChecked(const char* cmd, char* result)
    {
      if (!executeCommand(cmd, result))
      {
        return false;
      }
      return checkErrorState();
    }

    bool checkErrorState()
    {
      char result[40] = "";
      if (!executeCommand(":GE#", result))
      {
        return false;
      }
      int cmderr = (int)CE_NONE;
      if (!atoi2(result, &cmderr))
      {
        setError(ErrorCode::UnspecifiedError, "Cannot interpret command error");
        return false;
      }
      setError(commandErrorToCode(cmderr), commandErrorToString(cmderr));
      return errorCode == (int)ErrorCode::None;
    }

    void send()
    {
      jsonBody["ClientTransactionID"] = clientTransactionID;
      jsonBody["ServerTransactionID"] = 54321; // TODO how to generate this?

      String json;
      serializeJson(jsonBody, json);
      server.send(200, "application/json", json);
    }

    void sendWithError()
    {
      jsonBody["ClientTransactionID"] = clientTransactionID;
      jsonBody["ServerTransactionID"] = 54321; // TODO how to generate this?
      jsonBody["ErrorNumber"] = errorCode;
      jsonBody["ErrorMessage"] = errorMessage;

      String json;
      serializeJson(jsonBody, json);
      server.send(200, "application/json", json);
    }

  public:
    DynamicJsonDocument jsonParams;
    DynamicJsonDocument jsonBody;

  private:
    String jsonParamsBuffer;

    int clientID = 1;
    int clientTransactionID = 1234;
    int errorCode = (int)ErrorCode::None;
    String errorMessage = "";
  };

  String getManagementURL(const String& method, bool versioned)
  {
    if (versioned)
    {
      return String("/management/" API_VERSION "/") + method;
    }
    else
    {
      return String("/management/") + method;
    }
  }

  String getDeviceURL(const String& deviceType, int deviceNumber, const String& method)
  {
    return String("/api/" API_VERSION "/") + deviceType + "/" + String(deviceNumber) + "/" + method;
  }

  void handleAlpacaApiVersions()
  {
    AlpacaResponse r;
    if (r.init())
    {
      JsonArray value = r.jsonBody.createNestedArray("Value");
      value.add(1);

      r.send();
    }
  }

  void handleAlpacaDescription()
  {
    AlpacaResponse r;
    if (r.init())
    {
      JsonObject value = r.jsonBody.createNestedObject("Value");
      value["ServerName"] = ServerName;
      value["Manufacturer"] = Manufacturer;
      value["ManufacturerVersion"] = ManufacturerVersion;
      value["Location"] = Location;

      r.send();
    }
  }

  void handleAlpacaConfiguredDevices()
  {
    AlpacaResponse r;
    if (r.init())
    {
      JsonArray value = r.jsonBody.createNestedArray("Value");
      JsonObject jsonTelescope = value.createNestedObject();
      jsonTelescope["DeviceName"] = "Telescope";
      jsonTelescope["DeviceType"] = "telescope";
      jsonTelescope["DeviceNumber"] = 0;
      jsonTelescope["UniqueID"] = "9f90f379-114c-428f-a08e-852f51b3487e";

      r.send();
    }
  }

  void handleAlpacaTargetDeclination()
  {
    AlpacaResponse r;
    if (r.init())
    {
      if (server.method() == HTTP_GET)
      {
        // Get Currently Selected Target Declination
        char temp[40] = "";
        if (r.executeCommandChecked(":Gd#", temp))
        {
          double value;
          if (dmsToDouble(&value, temp, true))
          {
            r.jsonBody["Value"] = value;
          }
        }
        r.sendWithError();
      }
      else if (server.method() == HTTP_PUT)
      {
        double value;
        if (r.parseJsonArg("TargetDeclination", value))
        {
          // Set target object declination
          char cmd[40], temp[40]="";
          doubleToDms(temp, &value, false, true);
          sprintf(cmd, ":Sd%s#", temp);
          r.executeCommandChecked(cmd, temp);
          r.sendWithError();
        }
      }
    }
  }

  void handleAlpacaTargetRightAscension()
  {
    AlpacaResponse r;
    if (r.init())
    {
      if (server.method() == HTTP_GET)
      {
        // Get current/target object RA
        char temp[40] = "";
        if (r.executeCommandChecked(":Gr#", temp))
        {
          double value;
          if (hmsToDouble(&value, temp))
          {
            r.jsonBody["Value"] = value;
          }
        }
        r.sendWithError();
      }
      else if (server.method() == HTTP_PUT)
      {
        double value;
        if (r.parseJsonArg("TargetRightAscension", value))
        {
          // Set target object RA
          char cmd[40], temp[40]="";
          doubleToHms(temp, &value);
          sprintf(cmd, ":Sr%s#", temp);
          r.executeCommandChecked(cmd, temp);
          r.sendWithError();
        }
      }
    }
  }

} // namespace Alpaca

void setupAlpacaURIHandlers()
{
  server.on(Alpaca::getManagementURL("apiversions", false), Alpaca::handleAlpacaApiVersions);
  server.on(Alpaca::getManagementURL("description", true), Alpaca::handleAlpacaDescription);
  server.on(Alpaca::getManagementURL("configureddevices", true), Alpaca::handleAlpacaConfiguredDevices);

  server.on(Alpaca::getDeviceURL("telescope", 0, "targetdeclination"), Alpaca::handleAlpacaTargetDeclination);
  server.on(Alpaca::getDeviceURL("telescope", 0, "targetrightascension"), Alpaca::handleAlpacaTargetRightAscension);
}
