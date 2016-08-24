var xhrRequest = function (url, type, callback) {
  var xhr = new XMLHttpRequest();
  xhr.onload = function () {
    callback(this.responseText);
  };
  xhr.open(type, url);
  xhr.send();
};

function locationSuccess(pos) {
  
  //OpenWeatherMap personal free API key
  //Limited to 60 calls per minute with 95% reliability
  var myAPIKey = '530eec4da85490cf570490a371d86f22'; 
  
  // Construct URL
  var urlWeather = 'http://api.openweathermap.org/data/2.5/weather?lat=' +
      pos.coords.latitude + '&lon=' + pos.coords.longitude + '&appid=' + myAPIKey;
  
  console.log('WeatherUrl: ' + urlWeather);
  
  var urlForecast = 'http://api.openweathermap.org/data/2.5/forecast?lat=' +
      pos.coords.latitude + '&lon=' + pos.coords.longitude + '&appid=' + myAPIKey;
  console.log('FocusUrl: ' + urlForecast);

  // Send Weather request to OpenWeatherMap
  xhrRequest(urlWeather, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with weather info
      var json = JSON.parse(responseText);

      // Temperature in Kelvin, adjust to Fahrenheit
      var temperature = Math.round(json.main.temp * 9.00 / 5.00 - 459.67);
      console.log('Temperature is ' + temperature);

      // Conditions
      var conditions = json.weather[0].main;      
      console.log('Conditions are ' + conditions);
      
      // City
      var city = json.name;
      console.log('City is ' + city);
      
      // Assemble dictionary using our keys
      var dictionary = {};
      dictionary.TEMPERATURE = temperature;
      dictionary.CONDITIONS = conditions;
      dictionary.FORECAST_CITY = city;
      
      Pebble.sendAppMessage(dictionary,
       function(e) {
         console.log('Weather info sent to Pebble successfully!');
       },
       function(e) {
         console.log('Error sending weather info to Pebble!');
      }
    );
      
    }      
  );
  
    // Send forecast request to OpenWeatherMap
  xhrRequest(urlForecast, 'GET', 
    function(responseText) {
      // responseText contains a JSON object with forecast info
      var json = JSON.parse(responseText);
      
      //get forecast 3 to 6 hours from now
      var tempHr3 = Math.round(json.list[0].main.temp * 9.00 / 5.00 - 459.67);
      console.log('Hr3 temp is ' + tempHr3);
      var conditionHr3 = json.list[0].weather[0].description;
      console.log('Hr3 condition is ' + conditionHr3);

      var foundTomorrowMorning = false;
      var foundTomorrowEvening = false;
      var cnt = 0;
      var dateTime = new Date();
      var morningHour = 7;   // 7 am
      var eveningHour = 17;  // 5 pm
      var forecastHour = 0; // initialize
      
      //get tommorrow morning forecast
      var tempDay2 = 0.0;
      var conditionDay2 = 'Unknown';
      
      while (!foundTomorrowMorning)
      {
        dateTime = new Date(json.list[cnt].dt * 1000); //multiply by 1000 because Date() requires milliseconds
        forecastHour = dateTime.getHours();

        //get morning time, feed is in 3 hour increments so get feed that is within 2 hours of target hour
        if ( Math.abs(morningHour - forecastHour) < 2) {
            foundTomorrowMorning = true;
            conditionDay2 = json.list[cnt].weather[0].description;
            tempDay2 = Math.round(json.list[cnt].main.temp * 9.00 / 5.00 - 459.67);
            break;
          }
        
        cnt++; //increment count
        
        if (cnt >= json.list.length) {
            console.log(morningHour + ' hour not found in list of ' + cnt);
            break; //end of list, break from loop
          }
      }
      
      //get forecast about 48 hours from now
      var tempDay3 = 0.0;
      var conditionDay3 = 'Unknown';
      cnt = 0; //reset counter
      
      while (!foundTomorrowEvening)
      {
        dateTime = new Date(json.list[cnt].dt * 1000); //multiply by 1000 because Date() requires milliseconds
        forecastHour = dateTime.getHours();

        //get morning time, feed is in 3 hour increments so get feed that is within 2 hours of target hour
        if ( Math.abs(eveningHour - forecastHour) < 2) {
            foundTomorrowEvening = true;
            conditionDay3 = json.list[cnt].weather[0].description;
            tempDay3 = Math.round(json.list[cnt].main.temp * 9.00 / 5.00 - 459.67);
            break;
          }
        
        cnt++; //increment count
        
        if (cnt >= json.list.length) {
            console.log(eveningHour + 'hour not found in list of ' + cnt);
            break; //end of list, break from loop
          }
      }
      
      // Assemble dictionary using our keys
      var dictionary = {};
      dictionary.FORECAST_HR3_TEMPERATURE = tempHr3;
      dictionary.FORECAST_HR3_CONDITIONS = conditionHr3;
      dictionary.FORECAST_DAY2_TEMPERATURE = tempDay2;
      dictionary.FORECAST_DAY2_CONDITIONS = conditionDay2;
      dictionary.FORECAST_DAY3_TEMPERATURE = tempDay3;
      dictionary.FORECAST_DAY3_CONDITIONS = conditionDay3;
      
      Pebble.sendAppMessage(dictionary,
       function(e) {
         console.log('Forecast info sent to Pebble successfully!');
       },
       function(e) {
         console.log('Error sending forecast info to Pebble!');
      }
    );
    
    }   
  );
  
}


function locationError(err) {
  console.log('Error requesting location!');
}

function getWeather() {
  navigator.geolocation.getCurrentPosition(
    locationSuccess,
    locationError,
    {timeout: 15000, maximumAge: 60000}
  );
}

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
  function(e) {
    console.log('AppMessage received!');
    getWeather();
  }                     
);

// Listen for when the watchface is opened
Pebble.addEventListener('ready', 
  function(e) {
    console.log('PebbleKit JS ready!');

    // Get the initial weather
    getWeather();
  }
);