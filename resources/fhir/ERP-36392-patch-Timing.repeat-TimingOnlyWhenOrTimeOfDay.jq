((.snapshot,.differential)
    .element[]|
        select(.id=="Timing.repeat").constraint[]|
            select(.key=="TimingOnlyWhenOrTimeOfDay")
                .expression
) =
"(\n  %resource.ofType(MedicationRequest).dosageInstruction\n  | %resource.ofType(MedicationDispense).dosageInstruction\n  | %resource.ofType(MedicationStatement).dosage\n).all(\n    timing.repeat.frequency.exists() and\n    timing.repeat.period.exists() and\n    timing.repeat.periodUnit.exists() and\n    (timing.repeat.when.exists() or \n    timing.repeat.timeOfDay.exists())\n  implies\n  (\n    (\n      (%resource.ofType(MedicationRequest).exists() or %resource.ofType(MedicationDispense).exists())\n      implies\n      (%resource.dosageInstruction.timing.repeat.when.exists() xor %resource.dosageInstruction.timing.repeat.timeOfDay.exists())\n    )\n    and\n    (\n      %resource.ofType(MedicationStatement).exists()\n      implies\n      (%resource.dosage.timing.repeat.when.exists() xor %resource.dosage.timing.repeat.timeOfDay.exists())\n    )\n  )\n)"

