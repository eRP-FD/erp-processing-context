//////////////////////////////////////////////
// Copyright IBM Deutschland GmbH 2021, 2026
// (C) Copyright IBM Corp. 2021, 2026
//
// non-exclusively licensed to gematik GmbH
//////////////////////////////////////////////

import { DatePicker} from "./date-picker.js";

const counters = {
    fatal: 0,
    error: 0,
    warning: 0,
    information: 0
};
function selectData() {
    return {
        views: [],
        isValidating: false,
        pendingValidation: false,
        severityCounters: Object.assign({}, counters),
        showSeverity: {
            fatal: true,
            error: true,
            warning: true
        },
        validationParam: {
            view: '',
            date: null,
        },
        fhirDocument: null,
        offset: 0,
        validationResult: {},
        fetchViews() {
            fetch(`/views`)
            .then(response => {
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                return response.json();
            })
            .then(data => {
                this.views = data;
            })
            .catch(error => {
                this.views = ['ERROR: Could not load data'];
            });
        },
        setup() {
            Alpine.effect(() => {
                this.validate(this.validationParam, this.fhirDocument);
            });
        },
        validate(validationParam, fhirDocument) {
            if (!validationParam.view || !fhirDocument || !validationParam.date) {
                return;
            }
            if (this.isValidating) {
                this.pendingValidation = true;
                return;
            }
            this.fetchValidate(validationParam, fhirDocument);
        },
        fetchValidate(validationParam, fhirDocument) {
            fetch(`/validate?${new URLSearchParams(validationParam).toString()}`, {
                method: 'POST',
                body: fhirDocument
            })
            .then(response => {
                this.pendingValidation = false;
                this.isValidating = false;
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                return response.json();
            })
            .then(data => {
                this.validationResult = data;
                this.severityCounters = {};
                this.severityCounters = data.issue.reduce((acc, issue) => {
                    const severity = issue.severity;
                    acc[severity] = (acc[severity] || 0) + 1;
                    return acc;
                }, Object.assign({}, counters));
            })
            .catch(error => {
                this.validationResult = 'ERROR: Could not load data';
            });
        },
        offsetUpdated(validationDatePicker, offset) {
            let today = new Date();
            today.setHours(0, 0, 0, 0);
            validationDatePicker.setDate(new Date(today - Math.round(offset) * 86400000));
            this.validationParam.date = validationDatePicker.str();
        },
        validationDateUpdated(selected) {
            let today = new Date();
            today.setHours(0, 0, 0, 0);
            this.offset = Math.round((today - selected) / 86400000);
        },
        datePicker(el) {
            let picker = new DatePicker(el);
            picker.load().then(() => {
                Alpine.effect(() => {
                    this.validationDateUpdated(picker.date());
                });
                this.$watch('offset', offset => this.offsetUpdated(picker, offset));
                this.validationParam.date = picker.str();
            });
        }
    }
}

document.addEventListener('alpine:init', () => {
    Alpine.data('selectData', selectData);
});