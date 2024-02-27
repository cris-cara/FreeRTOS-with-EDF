#ifndef TYPES_H
#define TYPES_H

typedef enum {
    temperature,
    airHumidity,
    soilHumidity,
    motion,
    light,
} eSensorID;

char sensors_name[5][20] = {"temperature", "airHumidity", "soilHumidity", "motion", "light"};

/*That's the structure of data that tasks exchange among themself, containing the name of the sensor 
                as a constant (the enum) and the value read by the sensor*/
typedef struct {
    eSensorID eDataSource;
    int16_t value;
} Data_t;

#endif