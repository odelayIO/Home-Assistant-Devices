



### HA Template

```jinja2
{% if states("sensor.garage_door_status")|float <= 80 %}
  Garage Open
{% else %}
  Garage Closed
{% endif %}
```

