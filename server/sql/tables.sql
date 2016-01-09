create type pin_type as enum ('input', 'output');
create type severity as enum ('info', 'warning', 'error');

create table device (
    device_id serial primary key,
    name text unique not null,
    ip text not null,
    last_seen timestamp not null
);

create index device_name on device (name);

create table pin (
    pin_id serial primary key,
    device_id integer not null references device on delete cascade,
    name text not null,
    type pin_type not null,
    unique (device_id, name)
);

create index pin_name on pin (device_id, name);

create table log (
    device_id integer references device on delete cascade,
    pin_id integer references pin on delete cascade,
    severity severity not null,
    time timestamp not null,
    message text not null
);

