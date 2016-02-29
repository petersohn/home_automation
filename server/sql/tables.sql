create type pin_type as enum ('input', 'output');
create type severity as enum ('info', 'warning', 'error');
create type edge as enum ('rising', 'falling', 'both');


create table expression (
    expression_id serial primary key,
    value text not null
);


create table device (
    device_id serial primary key,
    name text unique not null,
    ip text not null,
    port integer not null,
    version integer not null,
    last_seen timestamp not null
);

create index device_name on device (name);


create table pin (
    pin_id serial primary key,
    device_id integer not null references device on delete cascade,
    name text not null,
    type pin_type not null,
    expression_id integer references expression on delete set null,
    unique (device_id, name)
);

create index pin_name on pin (device_id, name);


create table variable (
    variable_id serial primary key,
    name text unique not null,
    value integer not null
);

create index variable_name on variable (name);


create table input_trigger (
    input_trigger_id serial primary key,
    pin_id integer not null references pin on delete cascade,
    edge edge not null,
    expression_id integer not null references expression on delete cascade
);

create index input_trigger_pin_id on input_trigger (pin_id);


create table log (
    device_id integer references device on delete set null,
    pin_id integer references pin on delete set null,
    severity severity not null,
    time timestamp not null,
    message text not null
);

