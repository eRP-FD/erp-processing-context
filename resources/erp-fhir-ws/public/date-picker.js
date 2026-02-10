//////////////////////////////////////////////
// Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
//
// non-exclusively licensed to gematik GmbH
//////////////////////////////////////////////

const monthsDE = [ "Januar", "Februar", "März", "April"  , "Mai"     , "Juni",
                   "Juli"  , "August" , "September", "Oktober", "November", "Dezember"];
const daysDE = [ "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So" ];

export class DatePicker {
    #datePicker = null;
    #parent = null;
    #value = Alpine.reactive({   
        year: 0,
        month: 0,
        day: 0,
        date()
        {
            return new Date(this.year, this.month, this.day, 0, 0, 0, 0);
        }
    });
    constructor(parent)
    {
        this.#parent = parent;
    }

    async load()
    {
        return fetch("pines/date-picker.html")
            .then( res => res.text())
            .then( html => {
                this.#parent.innerHTML = html;
                this.#parent.querySelector('label').remove();
                let container = this.#parent.querySelector('.container');
                container.classList.remove("md:py-10");
                container.classList.remove("py-2");
                Alpine.initTree(this.#parent);
                this.#datePicker = Alpine.$data(this.#parent.querySelector('[x-data]'));
                this.#datePicker.datePickerFormat ='YYYY-MM-DD';
                this.#datePicker.datePickerMonthNames = monthsDE;
                this.#datePicker.datePickerDays = daysDE;
                this.#datePicker.$watch('datePickerValue', () => {
                    this.#value.year = this.#datePicker.datePickerYear;
                    this.#value.month = this.#datePicker.datePickerMonth;
                    this.#value.day = this.#datePicker.datePickerDay;
                });
                this.setDate(new Date())
        });
    }

    date()
    {
        return this.#value.date();
    }

    str()
    {
        return this.#datePicker.datePickerValue;
    }

    setDate(date)
    {
        this.#datePicker.datePickerMonth = date.getMonth();
        this.#datePicker.datePickerYear = date.getFullYear();
        this.#datePicker.datePickerDay = date.getDate();
        this.#datePicker.datePickerValue = this.#datePicker.datePickerFormatDate(date);
        this.#datePicker.datePickerCalculateDays();
    }
};
