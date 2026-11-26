module.exports = [
  {
    "type": "heading",
    "defaultValue": "Monokuma Appears!"
  },
  {
    // The red shape and the Seeds of Despair count; index.js fills it in.
    "type": "text",
    "id": "counter",
    "defaultValue": ""
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Appearance"
      },
      {
        "type": "radiogroup",
        "messageKey": "CLOCK_FONT",
        "label": "Clock font",
        "defaultValue": "0",
        "options": [
          { "label": "DorBlue", "value": "0" },
          { "label": "LECO (Pebble system font)", "value": "1" }
        ]
      }
    ]
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "Behavior"
      },
      {
        "type": "toggle",
        "messageKey": "SHAKE_ENABLED",
        "label": "Change image on shake",
        "description": "Shake or tap the watch to show Monokuma in a different pose.",
        "defaultValue": true
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
