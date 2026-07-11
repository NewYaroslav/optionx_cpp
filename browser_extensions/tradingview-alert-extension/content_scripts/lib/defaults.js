(function (root, factory) {
  if (typeof module !== "undefined" && module.exports) {
    module.exports = factory();
  } else {
    root.OptionXDefaults = factory();
  }
})(typeof self !== "undefined" ? self : this, function () {
  "use strict";
  return {
    DEFAULTS: {
      enabled: false,
      endpoint: "http://127.0.0.1:6560/api/v1/tradingview/signal",
      secret: "",
      include_tab_url: false,
      capture_alert_toasts: true,
      capture_private_alerts: true
    }
  };
});
