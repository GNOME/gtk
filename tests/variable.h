#ifndef __VARIABLE_H__
#define __VARAIBLE_H__

typedef struct
{
  double weight;
  double sum;
  double sum2;
} Variable;

#define VARIABLE_INIT { 0, 0.0, 0.0 }

void   variable_add_weighted       (Variable *variable,
                                    double    value,
                                    double    weight);
void   variable_add                (Variable *variable,
                                    double    value);
double variable_mean               (Variable *variable);
double variable_standard_deviation (Variable *variable);
void   variable_reset              (Variable *variable);

#endif /* __VARAIBLE_H__ */

