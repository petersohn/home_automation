create type pin_type as enum ('input', 'output');
create type severity as enum ('info', 'warning', 'error');
create type edge as enum ('rising', 'falling', 'both');


create table device (
    device_id serial primary key,
    name text unique not null,
    ip text not null,
    port integer not null,
    last_seen timestamp not null
);

create index device_name on device (name);


create table pin (
    pin_id serial primary key,
    device_id integer not null references device on delete cascade,
    name text not null,
    type pin_type not null,
    expression text,
    unique (device_id, name)
);

create index pin_name on pin (device_id, name);


create table parameter (
    parameter_id serial primary key,
    name text not null,
    value integer not null
);

create index parameter_name on parameter (name);


create table input_trigger (
    input_trigger_id serial primary key,
    pin_id integer not null references pin on delete cascade,
    edge edge not null,
    expression text not null
);

create index input_trigger_pin_id on input_trigger (pin_id);


create table log (
    device_id integer references device on delete cascade,
    pin_id integer references pin on delete cascade,
    severity severity not null,
    time timestamp not null,
    message text not null
);

