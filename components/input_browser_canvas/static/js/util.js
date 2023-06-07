function getDeviceInfo(successCallback, errorCallback) {
    $.ajax({
        type: "GET",
        dataType: "json",
        url: "/info/device.json",
        success: successCallback,
        error: errorCallback
      });
}