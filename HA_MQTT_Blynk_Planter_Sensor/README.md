# Wireless Moisture Sensor Mobile App



Created a wireless moisture sensor which updates the moisture level on a IOS/Android Blynk App.  This was achieved using the [Arduino Nano ESP32](https://docs.arduino.cc/tutorials/nano-esp32/cheat-sheet/) development board and [Blynk.io](https://blynk.io/) mobile app.



### Web Dashboard

![image-20240914121457494](./docs/image-20240914121457494.png)

### Mobile App

![20240914_191854000_iOS](./docs/20240914_191854000_iOS.png)

## Creating Blynk.Console on Blynk.io



### Created a Template for Arduino Nano ESP32 board

![image-20240914121620471](./docs/image-20240914121620471.png)



### Blynk Datastreams

![image-20240914121657159](./docs/image-20240914121657159.png)



### Blynk Web Dashboard

![image-20240914121722688](./docs/image-20240914121722688.png)



### Events & Notifications

![image-20240914121756175](./docs/image-20240914121756175.png)



### Blynk Automations

![image-20240914121834728](./docs/image-20240914121834728.png)







## Part List

| Part Description                 | Link                                                         | Notes                                            |
| -------------------------------- | ------------------------------------------------------------ | ------------------------------------------------ |
| Arduino Nano ESP32               | [Amazon](https://www.amazon.com/Arduino-ABX00083-Bluetooth-MicroPython-Compatible/dp/B0C947BHK5/ref=sr_1_1_sspa?crid=1LHE1BPO6T1CZ&dib=eyJ2IjoiMSJ9.GzP-GvhsR81ftjmV7C-hRSqxdi7Hlz6onnJy09CvVnB-e47WF9xx4PdkocILjZLZ4-p8eQGgZsy9phlsHUloZ3YA28UBmBsaXXY-kSq7HMtZ5QUDp_gFwZPFEXGRyoHEkffdMc9bPb648RKSEVs7udJ8znY6hNYCjK9ObsZ3cdsXiE72gL84Gq4tIUNoOeL0sCpZaEllcZuov405Dpj6fMbgXtQx9B_cRFzzrbXb1wE.Gd4dJN_rXKFQugccEcY8jmhJfDvop7U8hwzMWc4P4WQ&dib_tag=se&keywords=arduino+nano+esp32&qid=1725316810&sprefix=arduino+nano+esp32%2Caps%2C148&sr=8-1-spons&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&psc=1) |                                                  |
| Moisture Sensor LM393 3V3 to 5V0 | [Amazon](https://www.amazon.com/HiLetgo-Moisture-Automatic-Watering-Arduino/dp/B01DKISKLO/ref=sr_1_7?crid=385VNNPKGYP9Z&dib=eyJ2IjoiMSJ9.Rjq-jful1ChilKba7CvfTaS-a7HW40I4klVGc4dVpaEO5W0l1ykbPbeOfgeY5fQQ8qG74jjhlhqVhr2oHhCx4mVwh_ukMXXJ-hH_f3TVGiFgs5B4XULhke1zx4l9HmJYpHAS-uDcFrvh1-8VRKNABIUEGmirM2g9V51-e82l_ipeaTGd2YS6g7zuX6tdjwh1OzIu9djQ8eD3hVFfoRZbxhrBorkEY32Pmetzdc93mV0.618aWsLBCyyKAKvsJBXAoR_p9Cq2EB2yDsGL2XlIrpU&dib_tag=se&keywords=Soil+Moisture+Sensor&qid=1725316484&sprefix=soil+moisture+sensor%2Caps%2C135&sr=8-7) |                                                  |
| DC-DC Step up Power Module       | [Amazon](https://www.amazon.com/Comidox-Module-Voltage-Converter-0-9-5V/dp/B07L76KLRY/ref=sr_1_1?crid=8L59AUCHPTPV&dib=eyJ2IjoiMSJ9.pzjbPVYdYn3oi9dSMbHKqgPz-zK7e2UGQLyk7W1rekF5rwbqojQlIRZhxJRNTC9m57ZW_QxQAauhY5DquW8NJO2-tuF3wziDU56QPHYgeKENfbzlalE53JIuyPEncl3BQzRURcdB5DP_ojATs7qBR1axWapHSeJqnJv5anYQYBMZOYJTrLS9swagqEAVDQpaik8q6wh9RGzk8Cx0wK51y3VeSBvcghNffIVNqiaKvxo.pU5nT3HBZgaKZMnuzcvZkDD5rxWnGRhMecj5xy8H244&dib_tag=se&keywords=1.5V+1.8V+2.5V+3V+3.3V+3.7V+4.2V+to+5V+DC-DC+Step+Up+Power+Module+Voltage+Boost+Converter+Board+0.9-5V+to+5V+5PCS&qid=1725316674&sprefix=1.5v+1.8v+2.5v+3v+3.3v+3.7v+4.2v+to+5v+dc-dc+step+up+power+module+voltage+boost+converter+board+0.9-5v+to+5v+5pcs%2Caps%2C138&sr=8-1) | Optional when embedding into outdoor solar light |
| Outdoor Solar Light              | [Amazon](https://www.amazon.com/Outdoor-Waterproof-Powered-Security-Outside/dp/B0CMCKWZNN/ref=sr_1_3_sspa?crid=25VYANC3XXE05&dib=eyJ2IjoiMSJ9.daORrdqYLfwM_dNX85r1TSRyBJpTdWN-ZcjkIwM0OY0nHUzhopuFFpxV8PIwuVzJpGl1PPu-6DjGn1bAAfmqHHPFGahXmWxycMIiDsnmxja_f-9Zr-ahTTTwbe3AfgPCsw-J0mBM67VTqoNUIUTnx2-TfFNUU2YN6GSgB5Ll8JX42eJFhAU9yzlct15d42dbrlYwUFS8Y6jtVinhGU1ZXyLFd-pereDICt77K3ngYdaBtLw2b0rN3KnhQp8J_hY5rUK7lhdMkfe79J1A9urlRPagpWIzvCTJRMRaSpd4B4Q.KnsU8r6QEFFh9sxASJdo2UZ7NZEAVzy8MQl1x4CoC2o&dib_tag=se&keywords=Solar%2BLights%2BOutdoor%2BWaterproof%2BMotion%2BSensor%2C380%2BLED%2BSolar%2BPowered%2BFlood%2BLights%2B3000LM%2BSolar%2BSecurity%2BWall%2BLights%2Bwith%2B3%2BModes%2BIP65%2BWaterproof%2Bfor%2BOutside%2C%2BYard%2C%2BWall%2C%2BStreet%2C%2BPatio&qid=1725316765&s=hi&sprefix=solar%2Blights%2Boutdoor%2Bwaterproof%2Bmotion%2Bsensor%2C380%2Bled%2Bsolar%2Bpowered%2Bflood%2Blights%2B3000lm%2Bsolar%2Bsecurity%2Bwall%2Blights%2Bwith%2B3%2Bmodes%2Bip65%2Bwaterproof%2Bfor%2Boutside%2C%2Byard%2C%2Bwall%2C%2Bstreet%2C%2Bpatio%2Ctools%2C140&sr=1-3-spons&sp_csd=d2lkZ2V0TmFtZT1zcF9hdGY&th=1) | Optional when embedding into outdoor solar light |

